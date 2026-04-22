#pragma once
#include "graph.hpp"
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdint>

namespace benchmark {

// Algorithm function signature
using AlgorithmFunc = std::function<std::vector<Weight>(const Graph&, Vertex)>;

struct GraphTopologyStats {
    size_t graph_size = 0;
    size_t edge_count = 0;
    double avg_degree = 0.0;
    double degree_stddev = 0.0;
    double degree_gini = 0.0;
    size_t max_degree = 0;

    size_t component_count = 0;
    double giant_component_fraction = 0.0;

    size_t directed_reachable_from_source = 0;
    double source_reachable_fraction = 0.0;

    double avg_distance_hops_unweighted = 0.0;
    double approx_diameter_hops = 0.0;
    double avg_clustering_coefficient = 0.0;

    double edge_weight_mean = 0.0;
    double edge_weight_stddev = 0.0;
};

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

    GraphTopologyStats topology;

    // BMSSP telemetry (non-BMSSP runs remain zero)
    uint64_t bmssp_calls_total = 0;
    int bmssp_max_level = 0;
    uint64_t bmssp_find_pivots_calls = 0;
    double bmssp_mean_pivot_ratio = 0.0;
    double bmssp_mean_frontier_expansion = 0.0;
    uint64_t bmssp_pull_count = 0;
    double bmssp_pull_mean_batch = 0.0;
    uint64_t bmssp_queue_insert_count = 0;
    uint64_t bmssp_queue_erase_count = 0;
    uint64_t bmssp_queue_batchprepend_count = 0;

    // Dijkstra operation counters (algorithm-specific)
    uint64_t pq_push_count = 0;
    uint64_t pq_pop_count = 0;
    uint64_t pq_stale_pop_count = 0;
    uint64_t pq_relax_attempt_count = 0;
    uint64_t pq_relax_success_count = 0;
    uint64_t fib_insert_count = 0;
    uint64_t fib_extract_count = 0;
    uint64_t fib_decrease_key_count = 0;
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

GraphTopologyStats compute_graph_topology_stats(const Graph& graph, Vertex source);

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
