// =============================================================================
// Single-source shortest paths via the Bounded Multi-Source Shortest Path
// algorithm of Duan, Mao, Mao, Shu and Yin (2025).
//
// Reference
// ---------
//   Ran Duan, Jiayi Mao, Xiao Mao, Xinkai Shu, Longhui Yin.
//   "Breaking the Sorting Barrier for Directed Single-Source Shortest Paths."
//   STOC 2025.  https://arxiv.org/pdf/2504.17033
//
// Time complexity:  O(m log^{2/3} n)  on a constant-degree graph.
// Space complexity: O(n log^{1/3} n)
//
// Overview
// --------
// The algorithm solves a more general problem than SSSP: the Bounded
// Multi-Source Shortest Path (BMSSP) problem.  Given
//   - a set of source vertices S (all with current distance estimates),
//   - an upper bound B,
// BMSSP finds every vertex v reachable from S with d(v) < B, settles it
// (i.e. makes its estimate exact), and returns a boundary B' <= B such that
// all vertices with d(v) < B' are guaranteed complete.
//
// Three subroutines (matching the three algorithms in the paper) are
// implemented here as private member functions:
//
//   find_pivots  — Algorithm 1: k rounds of Bellman-Ford to shrink the
//                  source set S to a small "pivot" set P.
//
//   base_case    — Algorithm 2: plain Dijkstra, stops after settling k+1
//                  vertices; used at recursion level 0.
//
//   bmssp_rec    — Algorithm 3: the main recursive BMSSP procedure.
//
// Graph requirements
// ------------------
// The paper requires the input graph to have constant in- and out-degree.
// Call prepare_graph(true) to apply the standard constant-degree
// transformation (described in Section 2 of the paper) before running
// execute().  If your graph already has bounded degree, use
// prepare_graph(false) (the default).
//
// Usage
// -----
//   Solver solver(num_vertices);
//   solver.add_edge(u, v, weight);
//   ...
//   solver.prepare_graph();
//   auto [dist, pred] = solver.execute(source);
//   auto path = solver.reconstruct_path(target, pred);
// =============================================================================

#ifndef BMSSP_HPP
#define BMSSP_HPP

#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <vector>
#include <algorithm>
#include <cassert>
#include <numeric>
#include <stdexcept>
#include <cstdint>

#include "path_label.hpp"
#include "batch_pq.hpp"

namespace duan25 {

// Edge in the adjacency list.
struct Edge {
    int    to;
    double weight;
};

using AdjList = std::vector<std::vector<Edge>>;

// =============================================================================
class Solver {
public:

    // Construct a solver for a graph with `n` vertices (0-indexed).
    explicit Solver(int n);

    // Construct directly from an existing adjacency list.
    explicit Solver(const AdjList& adj);

    // Add a directed edge from `u` to `v` with non-negative weight `w`.
    void add_edge(int u, int v, double w);

    // Must be called once before execute().
    // If `apply_cd_transform` is true, applies the constant-degree
    // transformation from Section 2 of the paper.  Pass false if the graph
    // already has constant in/out-degree.
    void prepare_graph(bool apply_cd_transform = false);

    // Run SSSP from `source` (a real vertex id, 0-indexed).
    // Returns (distance_vector, predecessor_vector), both indexed by real
    // vertex id.  Unreachable vertices have distance INF.
    std::pair<std::vector<double>, std::vector<int>> execute(int source);

    // Internal graph size after prepare_graph(); useful for measuring the
    // constant-degree transformation overhead.
    std::size_t working_vertex_count() const;
    std::size_t working_edge_count() const;

    // Given the predecessor array returned by execute(), reconstruct the
    // shortest path from the source to `target` as a sequence of real vertex ids.
    // Returns {} if target is unreachable.
    std::vector<int> reconstruct_path(int target,
                                      const std::vector<int>& real_pred) const;

private:

    // ── Data members ──────────────────────────────────────────────────────────

    int      num_real_vertices;
    bool     cd_transform_applied = false;

    AdjList  input_adj;    // adjacency list as supplied by the caller
    AdjList  working_adj;  // adjacency list actually used by the algorithm

    // Node id maps.  After the CD transform the number of internal nodes may
    // exceed num_real_vertices, so both directions are stored explicitly.
    std::vector<int> real_to_internal;   // real vertex id  -> internal id
    std::vector<int> internal_to_real;   // internal id     -> real vertex id

    // SSSP state (indexed by internal node id).
    std::vector<double> dist_estimate;   // db[v] in the paper
    std::vector<int>    hop_count;       // number of hops on current best path
    std::vector<int>    predecessor;     // Pred[v] in the paper

    // Algorithm parameters (derived from working graph size).
    int k;   // k = floor( log^{1/3}(n) )
    int t;   // t = floor( log^{2/3}(n) )

    // One BatchPQ per recursion level (shared across sequential recursive calls
    // at the same level to avoid repeated allocations).
    std::vector<BatchPQ> level_pqs;

    // ── FindPivots state (Algorithm 1) ────────────────────────────────────────
    std::vector<int>   pivot_root;       // root[v]: root of v's BFS tree
    std::vector<int>   tree_size;        // number of vertices in each root's tree
    std::vector<int>   visit_stamp;      // generation counter per vertex
    int                pivot_call_id = 0; // incremented each FindPivots call

    // settled_level[v] = the recursion level at which v was last settled.
    std::vector<int>   settled_level;

    // ── Infinity constant ─────────────────────────────────────────────────────
    const double INF = std::numeric_limits<double>::max() / 10.0;

    // ── Private methods  ────────────────────────────────────────────────────────
    
    void remove_parallel_edges();
    void validate_input_graph() const;
    void validate_real_vertex(int v) const;
    void build_identity_node_map();
    void apply_constant_degree_transform();
    void allocate_algorithm_state();
    void reset_state();

    PathLabel label_of(int v) const;
    PathLabel relaxed_label(int u, int v, double w) const;
    void relax(int u, int v, double w);

    std::pair<std::vector<int>, std::vector<int>>
    find_pivots(PathLabel B, const std::vector<int>& S);

    std::pair<PathLabel, std::vector<int>>
    base_case(PathLabel B, int x);

    std::pair<PathLabel, std::vector<int>>
    bmssp_rec(int level, PathLabel B, const std::vector<int>& S);

    struct LevelStep {
        PathLabel bound;
        std::vector<int> completed;
    };

    struct PivotBatch {
        std::vector<int> pivots;
        std::vector<int> visited;
    };

    PivotBatch discover_pivots(PathLabel B, const std::vector<int>& seeds);
    LevelStep process_level(int level, PathLabel B, const std::vector<int>& seeds);
    void relax_from_completed_vertices(int level,
                                       PathLabel parent_bound,
                                       PathLabel child_pull_bound,
                                       PathLabel child_complete_bound,
                                       const std::vector<int>& completed,
                                       std::vector<Entry>& postponed,
                                       BatchPQ& queue);

    int real_predecessor_of(int v) const;
    std::pair<std::vector<double>, std::vector<int>> build_output() const;
};

} // namespace duan25

// Wrapper function declarations for compatibility with algorithms namespace
// (Implementations are in bmssp_duan25.cpp)
#include "graph.hpp"

namespace algorithms {

struct BMSSPTelemetry {
    bool enabled = false;
    uint64_t calls_total = 0;
    int max_level = 0;
    uint64_t find_pivots_calls = 0;
    double pivot_ratio_sum = 0.0;
    double frontier_expansion_sum = 0.0;
    uint64_t pull_count = 0;
    uint64_t pull_total_batch = 0;
    uint64_t queue_insert_count = 0;
    uint64_t queue_erase_count = 0;
    uint64_t queue_batchprepend_count = 0;
};

void set_bmssp_telemetry_enabled(bool enabled);
void reset_bmssp_telemetry();
BMSSPTelemetry get_bmssp_telemetry();

std::vector<Weight> bmssp(const Graph& graph, Vertex source);
Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target);
std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target);

} // namespace algorithms

#endif // BMSSP_HPP
