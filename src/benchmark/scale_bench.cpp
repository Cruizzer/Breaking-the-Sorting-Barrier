#include "graph_generator.hpp"
#include "benchmark/benchmark.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/bmssp.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::vector<size_t> sizes = {1000, 5000, 10000, 50000};
    std::vector<int> degrees = {4, 8};
    size_t trials = 3;

    const unsigned generator_seed = 42;
    const bool enforce_connected = false;
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
        << "binpq_push_count,binpq_pop_count,binpq_stale_pop_count,"
        << "binpq_relax_attempt_count,binpq_relax_success_count,"
        << "fib_insert_count,fib_extract_count,fib_decrease_key_count,"
        << "fib_relax_attempt_count,fib_relax_success_count,"
        << "bmssp_calls_total,bmssp_max_level,bmssp_find_pivots_calls,"
        << "bmssp_mean_pivot_ratio,bmssp_mean_frontier_expansion,bmssp_pull_count,"
        << "bmssp_pull_mean_batch,bmssp_queue_insert_count,bmssp_queue_erase_count,bmssp_queue_batchprepend_count\n";

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

        for (size_t t = 0; t < trials; ++t) {
            Graph g;
            if (type == "random") {
                g = generate_random_graph(N,
                                          param,
                                          1.0,
                                          100.0,
                                          generator_seed,
                                          enforce_connected,
                                          generator_max_retries);
            } else if (type == "erdos_renyi") {
                g = generate_erdos_renyi_graph(N,
                                               param,
                                               1.0,
                                               100.0,
                                               generator_seed,
                                               enforce_connected,
                                               generator_max_retries);
            } else if (type == "barabasi_albert") {
                size_t m_attach = static_cast<size_t>(std::max(1, param));
                g = generate_barabasi_albert_graph(N,
                                                   m_attach,
                                                   1.0,
                                                   100.0,
                                                   generator_seed,
                                                   ba_initial_clique);
            } else if (type == "grid") {
                size_t side = std::max<size_t>(1, (size_t)std::sqrt((double)N));
                g = generate_grid_graph(side, side);
            } else if (type == "road") {
                g = generate_road_network(N,
                                          10.0,
                                          1000.0,
                                          generator_seed,
                                          road_cross_edge_rate,
                                          road_branch_min,
                                          road_branch_max);
            }

            Vertex source = (t * 1234567) % g.size();

            auto comp = benchmark::compare_three_algorithms(
                algorithms::dijkstra,
                algorithms::dijkstra_fibonacci,
                algorithms::bmssp,
                g,
                source
            );

            total_d_ms += comp.dijkstra_result.execution_time_ms;
            total_fib_ms += comp.dijkstra_fibonacci_result.execution_time_ms;
            total_b_ms += comp.bmssp_result.execution_time_ms;
            total_speedup_binary += comp.bmssp_speedup_vs_dijkstra;
            total_speedup_fib += comp.bmssp_speedup_vs_dijkstra_fibonacci;
            total_fib_vs_binary += comp.dijkstra_fibonacci_speedup_vs_dijkstra;
            total_reachable += comp.dijkstra_result.reachable_vertices;
            total_avgdist += comp.dijkstra_result.avg_distance;
            total_maxdist += comp.dijkstra_result.max_distance;
            total_match_binary_fib += comp.dijkstra_and_fibonacci_match ? 1 : 0;
            total_match_binary_bmssp += comp.dijkstra_and_bmssp_match ? 1 : 0;
            total_match_fib_bmssp += comp.dijkstra_fibonacci_and_bmssp_match ? 1 : 0;

            const auto& topo = comp.dijkstra_result.topology;
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

            total_binpq_push_count += static_cast<double>(comp.dijkstra_result.pq_push_count);
            total_binpq_pop_count += static_cast<double>(comp.dijkstra_result.pq_pop_count);
            total_binpq_stale_pop_count += static_cast<double>(comp.dijkstra_result.pq_stale_pop_count);
            total_binpq_relax_attempt_count += static_cast<double>(comp.dijkstra_result.pq_relax_attempt_count);
            total_binpq_relax_success_count += static_cast<double>(comp.dijkstra_result.pq_relax_success_count);

            total_fib_insert_count += static_cast<double>(comp.dijkstra_fibonacci_result.fib_insert_count);
            total_fib_extract_count += static_cast<double>(comp.dijkstra_fibonacci_result.fib_extract_count);
            total_fib_decrease_key_count += static_cast<double>(comp.dijkstra_fibonacci_result.fib_decrease_key_count);
            total_fib_relax_attempt_count += static_cast<double>(comp.dijkstra_fibonacci_result.pq_relax_attempt_count);
            total_fib_relax_success_count += static_cast<double>(comp.dijkstra_fibonacci_result.pq_relax_success_count);

            total_bmssp_calls_total += static_cast<double>(comp.bmssp_result.bmssp_calls_total);
            total_bmssp_max_level += static_cast<double>(comp.bmssp_result.bmssp_max_level);
            total_bmssp_find_pivots_calls += static_cast<double>(comp.bmssp_result.bmssp_find_pivots_calls);
            total_bmssp_mean_pivot_ratio += comp.bmssp_result.bmssp_mean_pivot_ratio;
            total_bmssp_mean_frontier_expansion += comp.bmssp_result.bmssp_mean_frontier_expansion;
            total_bmssp_pull_count += static_cast<double>(comp.bmssp_result.bmssp_pull_count);
            total_bmssp_pull_mean_batch += comp.bmssp_result.bmssp_pull_mean_batch;
            total_bmssp_queue_insert_count += static_cast<double>(comp.bmssp_result.bmssp_queue_insert_count);
            total_bmssp_queue_erase_count += static_cast<double>(comp.bmssp_result.bmssp_queue_erase_count);
            total_bmssp_queue_batchprepend_count += static_cast<double>(comp.bmssp_result.bmssp_queue_batchprepend_count);
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
            << avg_bmssp_queue_batchprepend_count << "\n";

        std::cout << "Completed: " << type << " N=" << N << " param=" << param
                  << " -> d=" << avg_d << " ms, fib=" << avg_fib << " ms, b=" << avg_b << " ms\n";
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
