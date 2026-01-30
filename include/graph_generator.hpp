#pragma once
#include "graph.hpp"
#include <random>

// Generate a random sparse graph
// Parameters:
//   n: number of vertices
//   avg_degree: average edges per vertex (controls sparsity)
//   min_weight, max_weight: edge weight range
//   seed: random seed for reproducibility
Graph generate_random_graph(
    size_t n,
    double avg_degree = 4.0,
    double min_weight = 1.0,
    double max_weight = 100.0,
    unsigned seed = 42
);

// Generate a grid graph (common in pathfinding benchmarks)
Graph generate_grid_graph(
    size_t rows,
    size_t cols,
    double min_weight = 1.0,
    double max_weight = 10.0,
    unsigned seed = 42
);

// Generate a road-like graph (more realistic structure)
Graph generate_road_network(
    size_t n,
    double min_weight = 10.0,
    double max_weight = 1000.0,
    unsigned seed = 42
);
