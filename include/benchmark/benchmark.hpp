#pragma once
#include "graph.hpp"
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace benchmark {

// Algorithm function signature
using AlgorithmFunc = std::function<std::vector<Weight>(const Graph&, Vertex)>;

// Benchmark result for a single run
struct BenchmarkResult {
    std::string algorithm_name;
    size_t graph_size;
    size_t edge_count;
    double avg_degree;
    Vertex source_vertex;
    
    // Timing results (in microseconds)
    double execution_time_us;
    double execution_time_ms;

    // Final distance vector produced by the algorithm
    std::vector<Weight> distances;
    
    // Correctness metrics
    size_t reachable_vertices;
    size_t unreachable_vertices;
    double avg_distance;
    Weight max_distance;
    
    // Memory usage (optional)
    size_t memory_bytes;
};

// Benchmark comparison result
struct ComparisonResult {
    BenchmarkResult dijkstra_result;
    BenchmarkResult bmssp_result;
    double speedup_factor;  // bmssp_time / dijkstra_time
    bool results_match;     // Do both algorithms produce same distances?
};

// Three-way comparison result
struct ThreeWayComparisonResult {
    BenchmarkResult dijkstra_result;
    BenchmarkResult dijkstra_fibonacci_result;
    BenchmarkResult bmssp_result;
    double bmssp_speedup_vs_dijkstra;
    double bmssp_speedup_vs_dijkstra_fibonacci;
    double dijkstra_fibonacci_speedup_vs_dijkstra;
    bool dijkstra_and_fibonacci_match;
    bool dijkstra_and_bmssp_match;
    bool dijkstra_fibonacci_and_bmssp_match;
};

// Run a single algorithm benchmark
BenchmarkResult run_benchmark(
    const std::string& algorithm_name,
    AlgorithmFunc algorithm,
    const Graph& graph,
    Vertex source
);

// Run comparison between Dijkstra and BMSSP
ComparisonResult compare_algorithms(
    AlgorithmFunc dijkstra,
    AlgorithmFunc bmssp,
    const Graph& graph,
    Vertex source
);

// Run comparison between binary Dijkstra, Fibonacci Dijkstra, and BMSSP
ThreeWayComparisonResult compare_three_algorithms(
    AlgorithmFunc dijkstra,
    AlgorithmFunc dijkstra_fibonacci,
    AlgorithmFunc bmssp,
    const Graph& graph,
    Vertex source
);

// Run multiple trials and average results
BenchmarkResult run_multiple_trials(
    const std::string& algorithm_name,
    AlgorithmFunc algorithm,
    const Graph& graph,
    size_t num_trials = 10
);

// Print benchmark result
void print_result(const BenchmarkResult& result);

// Print comparison result
void print_comparison(const ComparisonResult& result);

// Print three-way comparison result
void print_comparison(const ThreeWayComparisonResult& result);

// Generate performance report
void generate_report(const std::vector<ComparisonResult>& results, const std::string& filename);

// Generate performance report for three-way comparisons
void generate_report(const std::vector<ThreeWayComparisonResult>& results, const std::string& filename);

} // namespace benchmark
