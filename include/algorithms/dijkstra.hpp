#pragma once
#include "graph.hpp"
#include <vector>
#include <cstdint>

namespace algorithms {

struct DijkstraTelemetry {
	bool enabled = false;
	uint64_t binary_push_count = 0;
	uint64_t binary_pop_count = 0;
	uint64_t binary_stale_pop_count = 0;
	uint64_t relax_attempt_count = 0;
	uint64_t relax_success_count = 0;
	uint64_t fib_insert_count = 0;
	uint64_t fib_extract_count = 0;
	uint64_t fib_decrease_key_count = 0;
};

void set_dijkstra_telemetry_enabled(bool enabled);
void reset_dijkstra_telemetry();
DijkstraTelemetry get_dijkstra_telemetry();

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
