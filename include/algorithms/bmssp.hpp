#pragma once
#include "graph.hpp"
#include <vector>

namespace algorithms {

// Breaking the Monotone Sorting Separator (BMSSP) Algorithm
// A faster SSSP algorithm for sparse graphs
// 
// STUB IMPLEMENTATION - TO BE COMPLETED
// 
// Reference: "Breaking the O(m log n) barrier for SSSP in sparse graphs"
// This algorithm achieves better performance than Dijkstra on sparse graphs
// by avoiding the need for priority queue operations

// BMSSP shortest paths from source to all vertices
std::vector<Weight> bmssp(const Graph& graph, Vertex source);

// BMSSP with early termination for single target
Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target);

// BMSSP with path reconstruction
std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target);

} // namespace algorithms
