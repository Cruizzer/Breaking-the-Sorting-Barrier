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
