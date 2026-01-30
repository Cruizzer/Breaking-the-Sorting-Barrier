#include "benchmark/benchmark.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <numeric>
#include <cmath>
#include <random>

namespace benchmark {

static size_t count_edges(const Graph& graph) {
    size_t total = 0;
    for (const auto& adj_list : graph.adj) {
        total += adj_list.size();
    }
    return total;
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
    
    // Run algorithm and measure time
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Weight> distances = algorithm(graph, source);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.execution_time_us = duration.count();
    result.execution_time_ms = duration.count() / 1000.0;
    
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
    
    // Verify correctness - run Dijkstra and BMSSP again to get full results
    auto dijkstra_distances = dijkstra(graph, source);
    auto bmssp_distances = bmssp(graph, source);
    
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
    std::cout << "Graph: " << result.graph_size << " vertices, " 
              << result.edge_count << " edges (avg degree: " 
              << std::fixed << std::setprecision(2) << result.avg_degree << ")\n";
    std::cout << "Source vertex: " << result.source_vertex << "\n";
    std::cout << "Execution time: " << std::fixed << std::setprecision(3) 
              << result.execution_time_ms << " ms (" 
              << result.execution_time_us << " μs)\n";
    std::cout << "Reachable vertices: " << result.reachable_vertices 
              << " / " << result.graph_size << "\n";
    std::cout << "Average distance: " << std::fixed << std::setprecision(2) 
              << result.avg_distance << "\n";
    std::cout << "Max distance: " << std::fixed << std::setprecision(2) 
              << result.max_distance << "\n";
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
    } else if (result.speedup_factor < 1.0) {
        std::cout << "(SLOWER)\n";
    } else {
        std::cout << "(SAME)\n";
    }
    
    std::cout << "Results match: " << (result.results_match ? "YES ✓" : "NO ✗") << "\n";
    
    if (!result.results_match) {
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

} // namespace benchmark
