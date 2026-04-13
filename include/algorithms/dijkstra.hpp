#pragma once
#include "graph.hpp"
#include <vector>

namespace algorithms {

// Standard Dijkstra's algorithm implementation
// Computes shortest paths from source to all other vertices
std::vector<Weight> dijkstra(const Graph& graph, Vertex source);

// Dijkstra with early termination for single target
Weight dijkstra_single_target(const Graph& graph, Vertex source, Vertex target);

// Dijkstra with path reconstruction
std::vector<Vertex> dijkstra_path(const Graph& graph, Vertex source, Vertex target);

// Dijkstra implemented with a Fibonacci heap (decrease-key variant)
std::vector<Weight> dijkstra_fibonacci(const Graph& graph, Vertex source);

// Fibonacci-heap Dijkstra with early termination for single target
Weight dijkstra_fibonacci_single_target(const Graph& graph, Vertex source, Vertex target);

// Fibonacci-heap Dijkstra with path reconstruction
std::vector<Vertex> dijkstra_fibonacci_path(const Graph& graph, Vertex source, Vertex target);

} // namespace algorithms
