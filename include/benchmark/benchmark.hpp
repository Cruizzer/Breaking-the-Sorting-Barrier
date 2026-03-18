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

// Generate performance report
void generate_report(const std::vector<ComparisonResult>& results, const std::string& filename);

} // namespace benchmark
