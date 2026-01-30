#pragma once

#include <vector>
#include <cstddef>

using Weight = double;
using Vertex = std::size_t;

struct Edge {
    Vertex to;
    Weight weight;
};

// adjacency list
using Graph = std::vector<std::vector<Edge>>;

// Load graph from OSM file
Graph load_osm_graph(const char* filename);
