#include "graph_generator.hpp"
#include "benchmark/benchmark.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/bmssp.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <sstream>

namespace {

double mean_of(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double median_of(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    if ((n % 2) == 1) return values[n / 2];
    return 0.5 * (values[(n / 2) - 1] + values[n / 2]);
}

double stddev_of(const std::vector<double>& values) {
    if (values.size() <= 1) return 0.0;
    const double m = mean_of(values);
    double acc = 0.0;
    for (double x : values) {
        const double d = x - m;
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(values.size() - 1));
}

double p95_of(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t idx = static_cast<size_t>(std::ceil(0.95 * static_cast<double>(values.size()))) - 1;
    return values[std::min(idx, values.size() - 1)];
}

bool distances_match_rel_abs(const std::vector<Weight>& a, const std::vector<Weight>& b) {
    if (a.size() != b.size()) return false;
    const double abs_tol = 1e-6;
    const double rel_tol = 1e-9;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::isinf(a[i]) && std::isinf(b[i])) continue;
        const double scale = std::max(std::abs(a[i]), std::abs(b[i]));
        const double tol = abs_tol + rel_tol * scale;
        if (std::abs(a[i] - b[i]) > tol) return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<size_t> sizes = {1000, 5000, 10000, 50000};
    std::vector<int> degrees = {4, 8};
    size_t trials = 10;
    size_t warmup_runs = 1;
    std::string scale_profile = "baseline";

    bool sizes_overridden = false;
    bool trials_overridden = false;
    bool warmup_overridden = false;

    auto parse_sizes_csv = [](const std::string& csv) {
        std::vector<size_t> parsed;
        std::stringstream ss(csv);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                parsed.push_back(static_cast<size_t>(std::stoull(token)));
            }
        }
        return parsed;
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: scale_bench [options]\n"
                      << "  --scale baseline|large|xlarge\n"
                      << "  --sizes N1,N2,...\n"
                      << "  --trials N\n"
                      << "  --warmup N\n";
            return 0;
        }

        if (arg.rfind("--scale=", 0) == 0) {
            scale_profile = arg.substr(std::string("--scale=").size());
            continue;
        }
        if (arg == "--scale" && i + 1 < argc) {
            scale_profile = std::string(argv[++i]);
            continue;
        }

        if (arg.rfind("--sizes=", 0) == 0) {
            sizes = parse_sizes_csv(arg.substr(std::string("--sizes=").size()));
            sizes_overridden = !sizes.empty();
            continue;
        }
        if (arg == "--sizes" && i + 1 < argc) {
            sizes = parse_sizes_csv(std::string(argv[++i]));
            sizes_overridden = !sizes.empty();
            continue;
        }

        if (arg.rfind("--trials=", 0) == 0) {
            trials = static_cast<size_t>(std::stoull(arg.substr(std::string("--trials=").size())));
            trials_overridden = true;
            continue;
        }
        if (arg == "--trials" && i + 1 < argc) {
            trials = static_cast<size_t>(std::stoull(std::string(argv[++i])));
            trials_overridden = true;
            continue;
        }

        if (arg.rfind("--warmup=", 0) == 0) {
            warmup_runs = static_cast<size_t>(std::stoull(arg.substr(std::string("--warmup=").size())));
            warmup_overridden = true;
            continue;
        }
        if (arg == "--warmup" && i + 1 < argc) {
            warmup_runs = static_cast<size_t>(std::stoull(std::string(argv[++i])));
            warmup_overridden = true;
            continue;
        }
    }

    if (!sizes_overridden) {
        if (scale_profile == "large") {
            sizes = {100000, 250000, 500000, 1000000};
            if (!trials_overridden) trials = 5;
            if (!warmup_overridden) warmup_runs = 1;
        } else if (scale_profile == "xlarge") {
            sizes = {200000, 500000, 1000000, 2000000};
            if (!trials_overridden) trials = 3;
            if (!warmup_overridden) warmup_runs = 1;
        }
    }

    if (sizes.empty()) {
        std::cerr << "No sizes configured. Use --sizes N1,N2,...\n";
        return 1;
    }

    std::cout << "Scale profile: " << scale_profile << " | sizes=";
    for (size_t i = 0; i < sizes.size(); ++i) {
        std::cout << sizes[i] << (i + 1 < sizes.size() ? "," : "");
    }
    std::cout << " | trials=" << trials << " | warmup=" << warmup_runs << "\n";

    const unsigned generator_seed = 42;
    const bool enforce_connected = true;
    const size_t generator_max_retries = 20;
    const size_t ba_initial_clique = 0;
    const double road_cross_edge_rate = 0.10;
    const size_t road_branch_min = 1;
    const size_t road_branch_max = 3;

    std::ofstream out("scale_results.csv");
    out << "schema_version,graph_type,size,param,trials,seed,enforce_connected,max_retries,"
        << "ba_initial_clique,road_cross_edge_rate,road_branch_min,road_branch_max,"
        << "dijkstra_ms,dijkstra_fib_ms,bmssp_ms,"
        << "bmssp_vs_binary_speedup,bmssp_vs_fib_speedup,fib_vs_binary_speedup,"
        << "match_binary_fib,match_binary_bmssp,match_fib_bmssp,reachable,avg_distance,max_distance,"
        << "graph_avg_degree,graph_degree_stddev,graph_degree_gini,graph_max_degree,"
        << "graph_component_count,graph_giant_component_fraction,graph_source_reachable_fraction,"
        << "graph_avg_distance_hops_unweighted,graph_approx_diameter_hops,graph_avg_clustering_coefficient,"
        << "graph_edge_weight_mean,graph_edge_weight_stddev,"
        << "bmssp_id_prep_ms,bmssp_cd_prep_ms,bmssp_cd_internal_vertices,bmssp_cd_internal_edges,"
        << "bmssp_cd_vertex_blowup,bmssp_cd_edge_blowup,bmssp_cd_vs_identity_ratio,"
        << "binpq_push_count,binpq_pop_count,binpq_stale_pop_count,"
        << "binpq_relax_attempt_count,binpq_relax_success_count,"
        << "fib_insert_count,fib_extract_count,fib_decrease_key_count,"
        << "fib_relax_attempt_count,fib_relax_success_count,"
        << "bmssp_calls_total,bmssp_max_level,bmssp_find_pivots_calls,"
        << "bmssp_mean_pivot_ratio,bmssp_mean_frontier_expansion,bmssp_pull_count,"
        << "bmssp_pull_mean_batch,bmssp_queue_insert_count,bmssp_queue_erase_count,bmssp_queue_batchprepend_count,"
        << "warmup_runs,order_randomized,"
        << "dijkstra_median_ms,dijkstra_std_ms,dijkstra_p95_ms,"
        << "dijkstra_fib_median_ms,dijkstra_fib_std_ms,dijkstra_fib_p95_ms,"
        << "bmssp_median_ms,bmssp_std_ms,bmssp_p95_ms,"
        << "bmssp_total_with_cd_ms,bmssp_total_with_cd_median_ms,bmssp_total_with_cd_std_ms,bmssp_total_with_cd_p95_ms,"
        << "match_count_binary_fib,match_count_binary_bmssp,match_count_fib_bmssp\n";

    auto run_one = [&](const std::string& type, size_t N, int param){
        double total_d_ms = 0.0;
        double total_fib_ms = 0.0;
        double total_b_ms = 0.0;
        double total_speedup_binary = 0.0;
        double total_speedup_fib = 0.0;
        double total_fib_vs_binary = 0.0;
        size_t total_reachable = 0;
        double total_avgdist = 0.0;
        double total_maxdist = 0.0;
        size_t total_match_binary_fib = 0;
        size_t total_match_binary_bmssp = 0;
        size_t total_match_fib_bmssp = 0;

        double total_graph_avg_degree = 0.0;
        double total_graph_degree_stddev = 0.0;
        double total_graph_degree_gini = 0.0;
        double total_graph_max_degree = 0.0;
        double total_graph_component_count = 0.0;
        double total_graph_giant_component_fraction = 0.0;
        double total_graph_source_reachable_fraction = 0.0;
        double total_graph_avg_distance_hops_unweighted = 0.0;
        double total_graph_approx_diameter_hops = 0.0;
        double total_graph_avg_clustering_coefficient = 0.0;
        double total_graph_edge_weight_mean = 0.0;
        double total_graph_edge_weight_stddev = 0.0;
        double total_graph_vertex_count = 0.0;
        double total_graph_edge_count = 0.0;

        double total_bmssp_id_prep_ms = 0.0;
        double total_bmssp_cd_prep_ms = 0.0;
        double total_bmssp_cd_internal_vertices = 0.0;
        double total_bmssp_cd_internal_edges = 0.0;

        double total_binpq_push_count = 0.0;
        double total_binpq_pop_count = 0.0;
        double total_binpq_stale_pop_count = 0.0;
        double total_binpq_relax_attempt_count = 0.0;
        double total_binpq_relax_success_count = 0.0;

        double total_fib_insert_count = 0.0;
        double total_fib_extract_count = 0.0;
        double total_fib_decrease_key_count = 0.0;
        double total_fib_relax_attempt_count = 0.0;
        double total_fib_relax_success_count = 0.0;

        double total_bmssp_calls_total = 0.0;
        double total_bmssp_max_level = 0.0;
        double total_bmssp_find_pivots_calls = 0.0;
        double total_bmssp_mean_pivot_ratio = 0.0;
        double total_bmssp_mean_frontier_expansion = 0.0;
        double total_bmssp_pull_count = 0.0;
        double total_bmssp_pull_mean_batch = 0.0;
        double total_bmssp_queue_insert_count = 0.0;
        double total_bmssp_queue_erase_count = 0.0;
        double total_bmssp_queue_batchprepend_count = 0.0;

        std::vector<double> dijkstra_times_ms;
        std::vector<double> dijkstra_fib_times_ms;
        std::vector<double> bmssp_times_ms;
        std::vector<double> bmssp_total_with_cd_times_ms;
        dijkstra_times_ms.reserve(trials);
        dijkstra_fib_times_ms.reserve(trials);
        bmssp_times_ms.reserve(trials);
        bmssp_total_with_cd_times_ms.reserve(trials);

        for (size_t t = 0; t < trials; ++t) {
            Graph g;
            unsigned trial_seed = generator_seed
                + static_cast<unsigned>(t)
                + static_cast<unsigned>(N * 13)
                + static_cast<unsigned>(param * 97);
            if (type == "random") {
                g = generate_random_graph(N,
                                          param,
                                          1.0,
                                          100.0,
                                          trial_seed,
                                          enforce_connected,
                                          generator_max_retries);
            } else if (type == "erdos_renyi") {
                g = generate_erdos_renyi_graph(N,
                                               param,
                                               1.0,
                                               100.0,
                                               trial_seed,
                                               enforce_connected,
                                               generator_max_retries);
            } else if (type == "barabasi_albert") {
                size_t m_attach = static_cast<size_t>(std::max(1, param));
                g = generate_barabasi_albert_graph(N,
                                                   m_attach,
                                                   1.0,
                                                   100.0,
                                                   trial_seed,
                                                   ba_initial_clique);
            } else if (type == "grid") {
                size_t side = std::max<size_t>(1, (size_t)std::sqrt((double)N));
                g = generate_grid_graph(side, side);
            } else if (type == "road") {
                g = generate_road_network(N,
                                          10.0,
                                          1000.0,
                                          trial_seed,
                                          road_cross_edge_rate,
                                          road_branch_min,
                                          road_branch_max);
            }

            Vertex source = ((t + 1) * 1234567 + N + static_cast<size_t>(param)) % g.size();

            enum AlgIndex : int { BIN = 0, FIB = 1, BMSSP = 2 };
            std::array<int, 3> order = {BIN, FIB, BMSSP};
            std::mt19937 order_rng(trial_seed ^ 0x9e3779b9U);

            auto run_index = [&](int index) {
                if (index == BIN) {
                    return benchmark::run_benchmark("Dijkstra", algorithms::dijkstra, g, source);
                }
                if (index == FIB) {
                    return benchmark::run_benchmark("Dijkstra Fibonacci", algorithms::dijkstra_fibonacci, g, source);
                }
                return benchmark::run_benchmark("BMSSP", algorithms::bmssp, g, source);
            };

            for (size_t w = 0; w < warmup_runs; ++w) {
                std::shuffle(order.begin(), order.end(), order_rng);
                for (int idx : order) {
                    (void)run_index(idx);
                }
            }

            std::array<benchmark::BenchmarkResult, 3> results;
            std::shuffle(order.begin(), order.end(), order_rng);
            for (int idx : order) {
                results[static_cast<size_t>(idx)] = run_index(idx);
            }

            const auto& dijkstra_result = results[BIN];
            const auto& dijkstra_fib_result = results[FIB];
            const auto& bmssp_result = results[BMSSP];

            const bool match_binary_fib = distances_match_rel_abs(dijkstra_result.distances, dijkstra_fib_result.distances);
            const bool match_binary_bmssp = distances_match_rel_abs(dijkstra_result.distances, bmssp_result.distances);
            const bool match_fib_bmssp = distances_match_rel_abs(dijkstra_fib_result.distances, bmssp_result.distances);

            total_d_ms += dijkstra_result.execution_time_ms;
            total_fib_ms += dijkstra_fib_result.execution_time_ms;
            total_b_ms += bmssp_result.execution_time_ms;
            total_speedup_binary += dijkstra_result.execution_time_us / bmssp_result.execution_time_us;
            total_speedup_fib += dijkstra_fib_result.execution_time_us / bmssp_result.execution_time_us;
            total_fib_vs_binary += dijkstra_result.execution_time_us / dijkstra_fib_result.execution_time_us;
            total_reachable += dijkstra_result.reachable_vertices;
            total_avgdist += dijkstra_result.avg_distance;
            total_maxdist += dijkstra_result.max_distance;
            total_match_binary_fib += match_binary_fib ? 1 : 0;
            total_match_binary_bmssp += match_binary_bmssp ? 1 : 0;
            total_match_fib_bmssp += match_fib_bmssp ? 1 : 0;

            dijkstra_times_ms.push_back(dijkstra_result.execution_time_ms);
            dijkstra_fib_times_ms.push_back(dijkstra_fib_result.execution_time_ms);
            bmssp_times_ms.push_back(bmssp_result.execution_time_ms);

            const auto& topo = dijkstra_result.topology;
            total_graph_avg_degree += topo.avg_degree;
            total_graph_degree_stddev += topo.degree_stddev;
            total_graph_degree_gini += topo.degree_gini;
            total_graph_max_degree += static_cast<double>(topo.max_degree);
            total_graph_component_count += static_cast<double>(topo.component_count);
            total_graph_giant_component_fraction += topo.giant_component_fraction;
            total_graph_source_reachable_fraction += topo.source_reachable_fraction;
            total_graph_avg_distance_hops_unweighted += topo.avg_distance_hops_unweighted;
            total_graph_approx_diameter_hops += topo.approx_diameter_hops;
            total_graph_avg_clustering_coefficient += topo.avg_clustering_coefficient;
            total_graph_edge_weight_mean += topo.edge_weight_mean;
            total_graph_edge_weight_stddev += topo.edge_weight_stddev;
            total_graph_vertex_count += static_cast<double>(dijkstra_result.graph_size);
            total_graph_edge_count += static_cast<double>(dijkstra_result.edge_count);

            auto id_prep = benchmark::measure_bmssp_constant_degree_preparation(g, source, false);
            total_bmssp_id_prep_ms += id_prep.preparation_time_ms;

            auto cd_prep = benchmark::measure_bmssp_constant_degree_preparation(g, source, true);
            total_bmssp_cd_prep_ms += cd_prep.preparation_time_ms;
            total_bmssp_cd_internal_vertices += static_cast<double>(cd_prep.internal_graph_vertices);
            total_bmssp_cd_internal_edges += static_cast<double>(cd_prep.internal_graph_edges);
            bmssp_total_with_cd_times_ms.push_back(bmssp_result.execution_time_ms + cd_prep.preparation_time_ms);

            total_binpq_push_count += static_cast<double>(dijkstra_result.pq_push_count);
            total_binpq_pop_count += static_cast<double>(dijkstra_result.pq_pop_count);
            total_binpq_stale_pop_count += static_cast<double>(dijkstra_result.pq_stale_pop_count);
            total_binpq_relax_attempt_count += static_cast<double>(dijkstra_result.pq_relax_attempt_count);
            total_binpq_relax_success_count += static_cast<double>(dijkstra_result.pq_relax_success_count);

            total_fib_insert_count += static_cast<double>(dijkstra_fib_result.fib_insert_count);
            total_fib_extract_count += static_cast<double>(dijkstra_fib_result.fib_extract_count);
            total_fib_decrease_key_count += static_cast<double>(dijkstra_fib_result.fib_decrease_key_count);
            total_fib_relax_attempt_count += static_cast<double>(dijkstra_fib_result.pq_relax_attempt_count);
            total_fib_relax_success_count += static_cast<double>(dijkstra_fib_result.pq_relax_success_count);

            total_bmssp_calls_total += static_cast<double>(bmssp_result.bmssp_calls_total);
            total_bmssp_max_level += static_cast<double>(bmssp_result.bmssp_max_level);
            total_bmssp_find_pivots_calls += static_cast<double>(bmssp_result.bmssp_find_pivots_calls);
            total_bmssp_mean_pivot_ratio += bmssp_result.bmssp_mean_pivot_ratio;
            total_bmssp_mean_frontier_expansion += bmssp_result.bmssp_mean_frontier_expansion;
            total_bmssp_pull_count += static_cast<double>(bmssp_result.bmssp_pull_count);
            total_bmssp_pull_mean_batch += bmssp_result.bmssp_pull_mean_batch;
            total_bmssp_queue_insert_count += static_cast<double>(bmssp_result.bmssp_queue_insert_count);
            total_bmssp_queue_erase_count += static_cast<double>(bmssp_result.bmssp_queue_erase_count);
            total_bmssp_queue_batchprepend_count += static_cast<double>(bmssp_result.bmssp_queue_batchprepend_count);
        }

        double avg_d = total_d_ms / trials;
        double avg_fib = total_fib_ms / trials;
        double avg_b = total_b_ms / trials;
        double avg_speed_binary = total_speedup_binary / trials;
        double avg_speed_fib = total_speedup_fib / trials;
        double avg_fib_vs_binary = total_fib_vs_binary / trials;
        double avg_reachable = static_cast<double>(total_reachable) / trials;
        double avg_dist = total_avgdist / trials;
        double avg_max = total_maxdist / trials;

        double avg_graph_avg_degree = total_graph_avg_degree / trials;
        double avg_graph_degree_stddev = total_graph_degree_stddev / trials;
        double avg_graph_degree_gini = total_graph_degree_gini / trials;
        double avg_graph_max_degree = total_graph_max_degree / trials;
        double avg_graph_component_count = total_graph_component_count / trials;
        double avg_graph_giant_component_fraction = total_graph_giant_component_fraction / trials;
        double avg_graph_source_reachable_fraction = total_graph_source_reachable_fraction / trials;
        double avg_graph_avg_distance_hops_unweighted = total_graph_avg_distance_hops_unweighted / trials;
        double avg_graph_approx_diameter_hops = total_graph_approx_diameter_hops / trials;
        double avg_graph_avg_clustering_coefficient = total_graph_avg_clustering_coefficient / trials;
        double avg_graph_edge_weight_mean = total_graph_edge_weight_mean / trials;
        double avg_graph_edge_weight_stddev = total_graph_edge_weight_stddev / trials;
        double avg_graph_vertex_count = total_graph_vertex_count / trials;
        double avg_graph_edge_count = total_graph_edge_count / trials;

        double avg_bmssp_id_prep_ms = total_bmssp_id_prep_ms / trials;
        double avg_bmssp_cd_prep_ms = total_bmssp_cd_prep_ms / trials;
        double avg_bmssp_cd_internal_vertices = total_bmssp_cd_internal_vertices / trials;
        double avg_bmssp_cd_internal_edges = total_bmssp_cd_internal_edges / trials;
        double avg_bmssp_cd_vertex_blowup = avg_graph_vertex_count > 0.0
            ? avg_bmssp_cd_internal_vertices / avg_graph_vertex_count
            : 0.0;
        double avg_bmssp_cd_edge_blowup = avg_graph_edge_count > 0.0
            ? avg_bmssp_cd_internal_edges / avg_graph_edge_count
            : 0.0;
        double avg_bmssp_cd_vs_identity_ratio = avg_bmssp_id_prep_ms > 0.0
            ? avg_bmssp_cd_prep_ms / avg_bmssp_id_prep_ms
            : 0.0;

        double avg_binpq_push_count = total_binpq_push_count / trials;
        double avg_binpq_pop_count = total_binpq_pop_count / trials;
        double avg_binpq_stale_pop_count = total_binpq_stale_pop_count / trials;
        double avg_binpq_relax_attempt_count = total_binpq_relax_attempt_count / trials;
        double avg_binpq_relax_success_count = total_binpq_relax_success_count / trials;

        double avg_fib_insert_count = total_fib_insert_count / trials;
        double avg_fib_extract_count = total_fib_extract_count / trials;
        double avg_fib_decrease_key_count = total_fib_decrease_key_count / trials;
        double avg_fib_relax_attempt_count = total_fib_relax_attempt_count / trials;
        double avg_fib_relax_success_count = total_fib_relax_success_count / trials;

        double avg_bmssp_calls_total = total_bmssp_calls_total / trials;
        double avg_bmssp_max_level = total_bmssp_max_level / trials;
        double avg_bmssp_find_pivots_calls = total_bmssp_find_pivots_calls / trials;
        double avg_bmssp_mean_pivot_ratio = total_bmssp_mean_pivot_ratio / trials;
        double avg_bmssp_mean_frontier_expansion = total_bmssp_mean_frontier_expansion / trials;
        double avg_bmssp_pull_count = total_bmssp_pull_count / trials;
        double avg_bmssp_pull_mean_batch = total_bmssp_pull_mean_batch / trials;
        double avg_bmssp_queue_insert_count = total_bmssp_queue_insert_count / trials;
        double avg_bmssp_queue_erase_count = total_bmssp_queue_erase_count / trials;
        double avg_bmssp_queue_batchprepend_count = total_bmssp_queue_batchprepend_count / trials;

        const double median_d = median_of(dijkstra_times_ms);
        const double std_d = stddev_of(dijkstra_times_ms);
        const double p95_d = p95_of(dijkstra_times_ms);

        const double median_f = median_of(dijkstra_fib_times_ms);
        const double std_f = stddev_of(dijkstra_fib_times_ms);
        const double p95_f = p95_of(dijkstra_fib_times_ms);

        const double median_b = median_of(bmssp_times_ms);
        const double std_b = stddev_of(bmssp_times_ms);
        const double p95_b = p95_of(bmssp_times_ms);

        const double avg_b_total_cd = mean_of(bmssp_total_with_cd_times_ms);
        const double median_b_total_cd = median_of(bmssp_total_with_cd_times_ms);
        const double std_b_total_cd = stddev_of(bmssp_total_with_cd_times_ms);
        const double p95_b_total_cd = p95_of(bmssp_total_with_cd_times_ms);

        out << "2" << ","
            << type << "," << N << "," << param << "," << trials << ","
            << generator_seed << ","
            << (enforce_connected ? "1" : "0") << ","
            << generator_max_retries << ","
            << ba_initial_clique << ","
            << road_cross_edge_rate << ","
            << road_branch_min << ","
            << road_branch_max << ","
            << avg_d << "," << avg_fib << "," << avg_b << ","
            << avg_speed_binary << "," << avg_speed_fib << ","
            << avg_fib_vs_binary << ","
            << (total_match_binary_fib == trials ? "YES" : "NO") << ","
            << (total_match_binary_bmssp == trials ? "YES" : "NO") << ","
            << (total_match_fib_bmssp == trials ? "YES" : "NO") << ","
            << avg_reachable << "," << avg_dist << "," << avg_max << ","
            << avg_graph_avg_degree << ","
            << avg_graph_degree_stddev << ","
            << avg_graph_degree_gini << ","
            << avg_graph_max_degree << ","
            << avg_graph_component_count << ","
            << avg_graph_giant_component_fraction << ","
            << avg_graph_source_reachable_fraction << ","
            << avg_graph_avg_distance_hops_unweighted << ","
            << avg_graph_approx_diameter_hops << ","
            << avg_graph_avg_clustering_coefficient << ","
            << avg_graph_edge_weight_mean << ","
            << avg_graph_edge_weight_stddev << ","
            << avg_bmssp_id_prep_ms << ","
            << avg_bmssp_cd_prep_ms << ","
            << avg_bmssp_cd_internal_vertices << ","
            << avg_bmssp_cd_internal_edges << ","
            << avg_bmssp_cd_vertex_blowup << ","
            << avg_bmssp_cd_edge_blowup << ","
            << avg_bmssp_cd_vs_identity_ratio << ","
            << avg_binpq_push_count << ","
            << avg_binpq_pop_count << ","
            << avg_binpq_stale_pop_count << ","
            << avg_binpq_relax_attempt_count << ","
            << avg_binpq_relax_success_count << ","
            << avg_fib_insert_count << ","
            << avg_fib_extract_count << ","
            << avg_fib_decrease_key_count << ","
            << avg_fib_relax_attempt_count << ","
            << avg_fib_relax_success_count << ","
            << avg_bmssp_calls_total << ","
            << avg_bmssp_max_level << ","
            << avg_bmssp_find_pivots_calls << ","
            << avg_bmssp_mean_pivot_ratio << ","
            << avg_bmssp_mean_frontier_expansion << ","
            << avg_bmssp_pull_count << ","
            << avg_bmssp_pull_mean_batch << ","
            << avg_bmssp_queue_insert_count << ","
            << avg_bmssp_queue_erase_count << ","
            << avg_bmssp_queue_batchprepend_count << ","
            << warmup_runs << ","
            << "1" << ","
            << median_d << "," << std_d << "," << p95_d << ","
            << median_f << "," << std_f << "," << p95_f << ","
            << median_b << "," << std_b << "," << p95_b << ","
            << avg_b_total_cd << "," << median_b_total_cd << "," << std_b_total_cd << "," << p95_b_total_cd << ","
            << total_match_binary_fib << ","
            << total_match_binary_bmssp << ","
            << total_match_fib_bmssp << "\n";

        std::cout << "Completed: " << type << " N=" << N << " param=" << param
                  << " -> d(avg/med)=" << avg_d << "/" << median_d
                  << " ms, fib(avg/med)=" << avg_fib << "/" << median_f
                  << " ms, b(avg/med)=" << avg_b << "/" << median_b << " ms\n";
    };

    // Random graphs
    for (auto N : sizes) {
        for (int deg : degrees) run_one("random", N, deg);
    }

    // Erdős–Rényi graphs (same generator, explicit label for plotting)
    for (auto N : sizes) {
        for (int deg : degrees) run_one("erdos_renyi", N, deg);
    }

    // Barabasi-Albert graphs
    std::vector<int> ba_params = {2, 4};
    for (auto N : sizes) {
        for (int m_attach : ba_params) run_one("barabasi_albert", N, m_attach);
    }

    // Grid graphs (use N as approximately nodes, degree param ignored)
    for (auto N : sizes) run_one("grid", N, 0);

    // Road-like graphs
    for (auto N : sizes) run_one("road", N, 0);

    out.close();
    std::cout << "Results written to scale_results.csv\n";
    return 0;
}
