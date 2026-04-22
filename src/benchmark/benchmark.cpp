#include "benchmark/benchmark.hpp"
#include "algorithms/bmssp.hpp"
#include "algorithms/dijkstra.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <numeric>
#include <cmath>
#include <random>
#include <queue>
#include <unordered_set>

namespace benchmark {

static size_t count_edges(const Graph& graph) {
    size_t total = 0;
    for (const auto& adj_list : graph.adj) {
        total += adj_list.size();
    }
    return total;
}

static bool distances_match(const std::vector<Weight>& a, const std::vector<Weight>& b) {
    if (a.size() != b.size()) return false;

    const double epsilon = 1e-6;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::isinf(a[i]) && std::isinf(b[i])) {
            continue;
        }

        double diff = std::abs(a[i] - b[i]);
        if (diff > epsilon) {
            return false;
        }
    }

    return true;
}

static std::vector<std::vector<size_t>> build_weak_adjacency(const Graph& graph) {
    std::vector<std::vector<size_t>> weak_adj(graph.size());
    for (size_t u = 0; u < graph.size(); ++u) {
        for (const auto& e : graph[u]) {
            weak_adj[u].push_back(e.to);
            weak_adj[e.to].push_back(u);
        }
    }
    return weak_adj;
}

static std::pair<size_t, int> bfs_farthest(const std::vector<std::vector<size_t>>& adj, size_t start) {
    if (adj.empty()) return {0, 0};

    std::vector<int> dist(adj.size(), -1);
    std::queue<size_t> q;
    q.push(start);
    dist[start] = 0;

    size_t farthest_vertex = start;
    int farthest_dist = 0;

    while (!q.empty()) {
        size_t u = q.front();
        q.pop();
        if (dist[u] > farthest_dist) {
            farthest_dist = dist[u];
            farthest_vertex = u;
        }
        for (size_t v : adj[u]) {
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                q.push(v);
            }
        }
    }

    return {farthest_vertex, farthest_dist};
}

GraphTopologyStats compute_graph_topology_stats(const Graph& graph, Vertex source) {
    GraphTopologyStats stats;
    stats.graph_size = graph.size();
    stats.edge_count = count_edges(graph);

    if (graph.empty()) {
        return stats;
    }

    stats.avg_degree = static_cast<double>(stats.edge_count) / static_cast<double>(stats.graph_size);

    std::vector<double> degrees(stats.graph_size, 0.0);
    std::vector<double> edge_weights;
    edge_weights.reserve(stats.edge_count);

    for (size_t u = 0; u < graph.size(); ++u) {
        degrees[u] = static_cast<double>(graph[u].size());
        stats.max_degree = std::max(stats.max_degree, graph[u].size());
        for (const auto& e : graph[u]) {
            edge_weights.push_back(e.weight);
        }
    }

    const double mean_degree = stats.avg_degree;
    double degree_sq_sum = 0.0;
    for (double d : degrees) {
        const double delta = d - mean_degree;
        degree_sq_sum += delta * delta;
    }
    stats.degree_stddev = std::sqrt(degree_sq_sum / static_cast<double>(degrees.size()));

    double degree_sum = std::accumulate(degrees.begin(), degrees.end(), 0.0);
    if (degree_sum > 0.0) {
        std::vector<double> sorted_degrees = degrees;
        std::sort(sorted_degrees.begin(), sorted_degrees.end());
        double weighted = 0.0;
        for (size_t i = 0; i < sorted_degrees.size(); ++i) {
            weighted += static_cast<double>(i + 1) * sorted_degrees[i];
        }
        const double n = static_cast<double>(sorted_degrees.size());
        stats.degree_gini = (2.0 * weighted) / (n * degree_sum) - (n + 1.0) / n;
    }

    if (!edge_weights.empty()) {
        const double weight_sum = std::accumulate(edge_weights.begin(), edge_weights.end(), 0.0);
        stats.edge_weight_mean = weight_sum / static_cast<double>(edge_weights.size());

        double weight_sq_sum = 0.0;
        for (double w : edge_weights) {
            double delta = w - stats.edge_weight_mean;
            weight_sq_sum += delta * delta;
        }
        stats.edge_weight_stddev = std::sqrt(weight_sq_sum / static_cast<double>(edge_weights.size()));
    }

    const auto weak_adj = build_weak_adjacency(graph);
    {
        std::vector<char> visited(graph.size(), 0);
        size_t giant = 0;
        for (size_t s = 0; s < graph.size(); ++s) {
            if (visited[s]) continue;
            stats.component_count++;
            std::queue<size_t> q;
            q.push(s);
            visited[s] = 1;
            size_t comp_size = 0;
            while (!q.empty()) {
                size_t u = q.front();
                q.pop();
                comp_size++;
                for (size_t v : weak_adj[u]) {
                    if (!visited[v]) {
                        visited[v] = 1;
                        q.push(v);
                    }
                }
            }
            giant = std::max(giant, comp_size);
        }
        stats.giant_component_fraction = static_cast<double>(giant) / static_cast<double>(graph.size());
    }

    if (source < graph.size()) {
        std::vector<int> dist(graph.size(), -1);
        std::queue<size_t> q;
        q.push(source);
        dist[source] = 0;
        while (!q.empty()) {
            size_t u = q.front();
            q.pop();
            for (const auto& e : graph[u]) {
                size_t v = e.to;
                if (dist[v] == -1) {
                    dist[v] = dist[u] + 1;
                    q.push(v);
                }
            }
        }

        double distance_sum = 0.0;
        size_t reached = 0;
        for (int d : dist) {
            if (d >= 0) {
                reached++;
                distance_sum += d;
            }
        }
        stats.directed_reachable_from_source = reached;
        stats.source_reachable_fraction = static_cast<double>(reached) / static_cast<double>(graph.size());
        stats.avg_distance_hops_unweighted = reached > 0 ? distance_sum / static_cast<double>(reached) : 0.0;
    }

    {
        size_t start = 0;
        while (start < graph.size() && weak_adj[start].empty()) start++;
        if (start < graph.size()) {
            auto [far_a, _] = bfs_farthest(weak_adj, start);
            auto [far_b, diameter] = bfs_farthest(weak_adj, far_a);
            (void)far_b;
            stats.approx_diameter_hops = static_cast<double>(diameter);
        }
    }

    {
        std::vector<std::unordered_set<size_t>> nbr_set(graph.size());
        for (size_t u = 0; u < graph.size(); ++u) {
            nbr_set[u].reserve(graph[u].size() * 2 + 1);
            for (const auto& e : graph[u]) {
                if (e.to != u) {
                    nbr_set[u].insert(e.to);
                }
            }
        }

        double local_sum = 0.0;
        size_t considered = 0;
        for (size_t u = 0; u < graph.size(); ++u) {
            std::vector<size_t> nbrs(nbr_set[u].begin(), nbr_set[u].end());
            const size_t k = nbrs.size();
            if (k < 2) continue;

            size_t links = 0;
            for (size_t i = 0; i < k; ++i) {
                for (size_t j = i + 1; j < k; ++j) {
                    if (nbr_set[nbrs[i]].count(nbrs[j]) || nbr_set[nbrs[j]].count(nbrs[i])) {
                        links++;
                    }
                }
            }

            const double possible = static_cast<double>(k) * static_cast<double>(k - 1) / 2.0;
            local_sum += static_cast<double>(links) / possible;
            considered++;
        }

        stats.avg_clustering_coefficient = considered > 0 ? local_sum / static_cast<double>(considered) : 0.0;
    }

    return stats;
}

BenchmarkResult run_benchmark(
    const std::string& algorithm_name,
    AlgorithmFunc algorithm,
    const Graph& graph,
    Vertex source
) {
    BenchmarkResult result;
    result.algorithm_name = algorithm_name;
    result.graph_size = graph.size();
    result.edge_count = count_edges(graph);
    result.avg_degree = graph.empty() ? 0.0 : static_cast<double>(result.edge_count) / graph.size();
    result.source_vertex = source;
    result.topology = compute_graph_topology_stats(graph, source);

    const bool is_bmssp = (algorithm_name == "BMSSP");
    const bool is_dijkstra = (algorithm_name == "Dijkstra");
    const bool is_dijkstra_fib = (algorithm_name == "Dijkstra Fibonacci");

    algorithms::set_bmssp_telemetry_enabled(is_bmssp);
    if (is_bmssp) {
        algorithms::reset_bmssp_telemetry();
    }

    algorithms::set_dijkstra_telemetry_enabled(is_dijkstra || is_dijkstra_fib);
    if (is_dijkstra || is_dijkstra_fib) {
        algorithms::reset_dijkstra_telemetry();
    }
    
    // Run algorithm and measure time
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Weight> distances = algorithm(graph, source);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.execution_time_us = duration.count();
    result.execution_time_ms = duration.count() / 1000.0;
    result.distances = distances;
    
    // Compute statistics
    result.reachable_vertices = 0;
    result.max_distance = 0.0;
    double sum_distance = 0.0;
    
    for (const auto& d : distances) {
        if (d != std::numeric_limits<Weight>::infinity()) {
            result.reachable_vertices++;
            sum_distance += d;
            if (d > result.max_distance) {
                result.max_distance = d;
            }
        }
    }
    
    result.unreachable_vertices = graph.size() - result.reachable_vertices;
    result.avg_distance = result.reachable_vertices > 0 ? sum_distance / result.reachable_vertices : 0.0;
    result.memory_bytes = distances.size() * sizeof(Weight);

    if (is_bmssp) {
        auto telemetry = algorithms::get_bmssp_telemetry();
        result.bmssp_calls_total = telemetry.calls_total;
        result.bmssp_max_level = telemetry.max_level;
        result.bmssp_find_pivots_calls = telemetry.find_pivots_calls;
        if (telemetry.find_pivots_calls > 0) {
            result.bmssp_mean_pivot_ratio = telemetry.pivot_ratio_sum /
                                            static_cast<double>(telemetry.find_pivots_calls);
            result.bmssp_mean_frontier_expansion = telemetry.frontier_expansion_sum /
                                                   static_cast<double>(telemetry.find_pivots_calls);
        }
        result.bmssp_pull_count = telemetry.pull_count;
        result.bmssp_pull_mean_batch = telemetry.pull_count > 0
            ? static_cast<double>(telemetry.pull_total_batch) / static_cast<double>(telemetry.pull_count)
            : 0.0;
        result.bmssp_queue_insert_count = telemetry.queue_insert_count;
        result.bmssp_queue_erase_count = telemetry.queue_erase_count;
        result.bmssp_queue_batchprepend_count = telemetry.queue_batchprepend_count;
    }

    if (is_dijkstra || is_dijkstra_fib) {
        auto telemetry = algorithms::get_dijkstra_telemetry();
        result.pq_push_count = telemetry.binary_push_count;
        result.pq_pop_count = telemetry.binary_pop_count;
        result.pq_stale_pop_count = telemetry.binary_stale_pop_count;
        result.pq_relax_attempt_count = telemetry.relax_attempt_count;
        result.pq_relax_success_count = telemetry.relax_success_count;
        result.fib_insert_count = telemetry.fib_insert_count;
        result.fib_extract_count = telemetry.fib_extract_count;
        result.fib_decrease_key_count = telemetry.fib_decrease_key_count;
    }
    
    return result;
}

ComparisonResult compare_algorithms(
    AlgorithmFunc dijkstra,
    AlgorithmFunc bmssp,
    const Graph& graph,
    Vertex source
) {
    ComparisonResult comparison;
    
    // Run Dijkstra
    comparison.dijkstra_result = run_benchmark("Dijkstra", dijkstra, graph, source);
    
    // Run BMSSP
    comparison.bmssp_result = run_benchmark("BMSSP", bmssp, graph, source);
    
    // Calculate speedup
    comparison.speedup_factor = comparison.dijkstra_result.execution_time_us / 
                                comparison.bmssp_result.execution_time_us;
    
    // Verify correctness - use stored distance vectors (avoid rerunning algorithms)
    const auto& dijkstra_distances = comparison.dijkstra_result.distances;
    const auto& bmssp_distances = comparison.bmssp_result.distances;

    comparison.results_match = true;
    const double epsilon = 1e-6;

    for (size_t i = 0; i < dijkstra_distances.size() && i < bmssp_distances.size(); ++i) {
        double diff = std::abs(dijkstra_distances[i] - bmssp_distances[i]);
        if (diff > epsilon && 
            !(std::isinf(dijkstra_distances[i]) && std::isinf(bmssp_distances[i]))) {
            comparison.results_match = false;
            break;
        }
    }
    
    return comparison;
}

ThreeWayComparisonResult compare_three_algorithms(
    AlgorithmFunc dijkstra,
    AlgorithmFunc dijkstra_fibonacci,
    AlgorithmFunc bmssp,
    const Graph& graph,
    Vertex source
) {
    ThreeWayComparisonResult comparison;

    comparison.dijkstra_result = run_benchmark("Dijkstra", dijkstra, graph, source);
    comparison.dijkstra_fibonacci_result = run_benchmark("Dijkstra Fibonacci", dijkstra_fibonacci, graph, source);
    comparison.bmssp_result = run_benchmark("BMSSP", bmssp, graph, source);

    comparison.bmssp_speedup_vs_dijkstra = comparison.dijkstra_result.execution_time_us /
                                           comparison.bmssp_result.execution_time_us;
    comparison.bmssp_speedup_vs_dijkstra_fibonacci = comparison.dijkstra_fibonacci_result.execution_time_us /
                                                     comparison.bmssp_result.execution_time_us;
    comparison.dijkstra_fibonacci_speedup_vs_dijkstra = comparison.dijkstra_result.execution_time_us /
                                                        comparison.dijkstra_fibonacci_result.execution_time_us;

    const auto& dijkstra_distances = comparison.dijkstra_result.distances;
    const auto& dijkstra_fibonacci_distances = comparison.dijkstra_fibonacci_result.distances;
    const auto& bmssp_distances = comparison.bmssp_result.distances;

    comparison.dijkstra_and_fibonacci_match = distances_match(dijkstra_distances, dijkstra_fibonacci_distances);
    comparison.dijkstra_and_bmssp_match = distances_match(dijkstra_distances, bmssp_distances);
    comparison.dijkstra_fibonacci_and_bmssp_match = distances_match(dijkstra_fibonacci_distances, bmssp_distances);

    return comparison;
}

BenchmarkResult run_multiple_trials(
    const std::string& algorithm_name,
    AlgorithmFunc algorithm,
    const Graph& graph,
    size_t num_trials
) {
    std::vector<BenchmarkResult> results;
    
    // Run multiple times from random source vertices
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, graph.size() - 1);
    
    for (size_t i = 0; i < num_trials; ++i) {
        Vertex source = dist(rng);
        results.push_back(run_benchmark(algorithm_name, algorithm, graph, source));
    }
    
    // Average the results
    BenchmarkResult avg_result = results[0];
    avg_result.execution_time_us = 0.0;
    avg_result.reachable_vertices = 0;
    avg_result.avg_distance = 0.0;
    
    for (const auto& r : results) {
        avg_result.execution_time_us += r.execution_time_us;
        avg_result.reachable_vertices += r.reachable_vertices;
        avg_result.avg_distance += r.avg_distance;
    }
    
    avg_result.execution_time_us /= num_trials;
    avg_result.execution_time_ms = avg_result.execution_time_us / 1000.0;
    avg_result.reachable_vertices /= num_trials;
    avg_result.avg_distance /= num_trials;
    
    return avg_result;
}

void print_result(const BenchmarkResult& result) {
    std::cout << "\n=== " << result.algorithm_name << " Results ===\n";
    std::cout << "Execution time: " << std::fixed << std::setprecision(3) 
              << result.execution_time_ms << " ms (" 
              << result.execution_time_us << " microseconds)\n";
    std::cout << "Reachable vertices: " << result.reachable_vertices << " / " << result.graph_size << "\n";
    std::cout << "Average distance: " << std::fixed << std::setprecision(2) << result.avg_distance << "\n";
    std::cout << "Max distance: " << std::fixed << std::setprecision(2) << result.max_distance << "\n";
    std::cout << "Final distances: ";
    for (const auto& d : result.distances) {
        if (std::isinf(d)) std::cout << "inf ";
        else std::cout << d << " ";
    }
    std::cout << "\n";
}

void print_comparison(const ComparisonResult& result) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "ALGORITHM COMPARISON\n";
    std::cout << std::string(70, '=') << "\n";
    
    print_result(result.dijkstra_result);
    print_result(result.bmssp_result);
    
    std::cout << "\n=== Performance Comparison ===\n";
    std::cout << "BMSSP Speedup: " << std::fixed << std::setprecision(2)
              << result.speedup_factor << "x ";

    if (result.speedup_factor > 1.0) {
        std::cout << "(FASTER)\n";
        std::cout << "BMSSP is " << std::fixed << std::setprecision(2)
                  << result.speedup_factor << "x faster than Dijkstra\n";
    } else if (result.speedup_factor < 1.0 && result.speedup_factor > 0.0) {
        double slower = 1.0 / result.speedup_factor;
        std::cout << "(SLOWER)\n";
        std::cout << "BMSSP is " << std::fixed << std::setprecision(2)
                  << slower << "x slower than Dijkstra\n";
    } else if (result.speedup_factor == 1.0) {
        std::cout << "(SAME)\n";
    } else {
        // Handle degenerate case (zero or negative)
        std::cout << "(UNDEFINED)\n";
    }
    
    std::cout << "Results match: " << (result.results_match ? "YES" : "NO") << "\n";
    
    if (!result.results_match) {
        std::cout << "WARNING: Algorithms produced different results!\n";
    }
    
    std::cout << std::string(70, '=') << "\n";
}

void print_comparison(const ThreeWayComparisonResult& result) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "ALGORITHM COMPARISON (THREE-WAY)\n";
    std::cout << std::string(70, '=') << "\n";

    print_result(result.dijkstra_result);
    print_result(result.dijkstra_fibonacci_result);
    print_result(result.bmssp_result);

    std::cout << "\n=== Performance Comparison ===\n";
    std::cout << "BMSSP vs Dijkstra speedup: " << std::fixed << std::setprecision(2)
              << result.bmssp_speedup_vs_dijkstra << "x\n";
    std::cout << "BMSSP vs Fibonacci speedup: " << std::fixed << std::setprecision(2)
              << result.bmssp_speedup_vs_dijkstra_fibonacci << "x\n";
    std::cout << "Fibonacci vs Dijkstra speedup: " << std::fixed << std::setprecision(2)
              << result.dijkstra_fibonacci_speedup_vs_dijkstra << "x\n";

    std::cout << "Results match (binary vs Fibonacci): "
              << (result.dijkstra_and_fibonacci_match ? "YES" : "NO") << "\n";
    std::cout << "Results match (binary vs BMSSP): "
              << (result.dijkstra_and_bmssp_match ? "YES" : "NO") << "\n";
    std::cout << "Results match (Fibonacci vs BMSSP): "
              << (result.dijkstra_fibonacci_and_bmssp_match ? "YES" : "NO") << "\n";

    if (!(result.dijkstra_and_fibonacci_match &&
          result.dijkstra_and_bmssp_match &&
          result.dijkstra_fibonacci_and_bmssp_match)) {
        std::cout << "WARNING: Algorithms produced different results!\n";
    }

    std::cout << std::string(70, '=') << "\n";
}

void generate_report(const std::vector<ComparisonResult>& results, const std::string& filename) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return;
    }
    
    file << "Graph Size,Edges,Avg Degree,Dijkstra (ms),BMSSP (ms),Speedup,Match\n";
    
    for (const auto& result : results) {
        file << result.dijkstra_result.graph_size << ","
             << result.dijkstra_result.edge_count << ","
             << std::fixed << std::setprecision(2) << result.dijkstra_result.avg_degree << ","
             << std::fixed << std::setprecision(3) << result.dijkstra_result.execution_time_ms << ","
             << std::fixed << std::setprecision(3) << result.bmssp_result.execution_time_ms << ","
             << std::fixed << std::setprecision(2) << result.speedup_factor << ","
             << (result.results_match ? "YES" : "NO") << "\n";
    }
    
    file.close();
    std::cout << "\nReport saved to: " << filename << "\n";
}

void generate_report(const std::vector<ThreeWayComparisonResult>& results, const std::string& filename) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return;
    }

    file << "Graph Size,Edges,Avg Degree,Dijkstra (ms),Dijkstra Fib (ms),BMSSP (ms),"
         << "BMSSP vs Dijkstra Speedup,BMSSP vs Dijkstra Fib Speedup,Fib vs Dijkstra Speedup,"
         << "Match Dijkstra/Fib,Match Dijkstra/BMSSP,Match Fib/BMSSP\n";

    for (const auto& result : results) {
        file << result.dijkstra_result.graph_size << ","
             << result.dijkstra_result.edge_count << ","
             << std::fixed << std::setprecision(2) << result.dijkstra_result.avg_degree << ","
             << std::fixed << std::setprecision(3) << result.dijkstra_result.execution_time_ms << ","
             << std::fixed << std::setprecision(3) << result.dijkstra_fibonacci_result.execution_time_ms << ","
             << std::fixed << std::setprecision(3) << result.bmssp_result.execution_time_ms << ","
             << std::fixed << std::setprecision(2) << result.bmssp_speedup_vs_dijkstra << ","
             << std::fixed << std::setprecision(2) << result.bmssp_speedup_vs_dijkstra_fibonacci << ","
             << std::fixed << std::setprecision(2) << result.dijkstra_fibonacci_speedup_vs_dijkstra << ","
             << (result.dijkstra_and_fibonacci_match ? "YES" : "NO") << ","
             << (result.dijkstra_and_bmssp_match ? "YES" : "NO") << ","
             << (result.dijkstra_fibonacci_and_bmssp_match ? "YES" : "NO") << "\n";
    }

    file.close();
    std::cout << "\nReport saved to: " << filename << "\n";
}

} // namespace benchmark
