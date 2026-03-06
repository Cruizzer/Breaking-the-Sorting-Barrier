#pragma once
#include "../graph.hpp"
#include "bmssp_expected.hpp"
#include "bmssp_wc.hpp"
#include <vector>

namespace algorithms {

// Breaking the Monotone Sorting Separator (BMSSP) Algorithm
// A faster SSSP algorithm for sparse graphs
// 
// Reference: "Breaking the O(m log n) barrier for SSSP in sparse graphs"
// This algorithm achieves better performance than Dijkstra on sparse graphs
// by avoiding the need for priority queue operations
//
// TWO VARIANTS PROVIDED:
// 1. BMSSP_Expected - expected-case complexity O(m + n log log C)
// 2. BMSSP_WorstCase - worst-case complexity guarantees
//
// NOTE: The old functional interface below is provided for backward compatibility.
// New code should use the BMSSP_Expected and BMSSP_WorstCase classes directly.

// DEPRECATED: Functional interface using expected-case algorithm
std::vector<Weight> bmssp(const Graph& graph, Vertex source);

// DEPRECATED: Functional interface using expected-case algorithm
Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target);

// DEPRECATED: Functional interface using expected-case algorithm
std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target);

} // namespace algorithms
