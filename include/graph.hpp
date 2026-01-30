#pragma once
#include <vector>
#include <cstddef>

// Vertex and weight types
using Vertex = std::size_t;
using Weight = double;

// Edge structure
struct Edge {
    Vertex to;
    Weight weight;
};

// Graph as adjacency list
using Graph = std::vector<std::vector<Edge>>;

// Loader declaration
Graph load_osm_graph(const char* filename);

// Optional generic alias
inline Graph load_graph(const char* filename) {
    return load_osm_graph(filename);
}

// Graph inspection utilities
void print_graph_info(const Graph& graph, size_t max_vertices = 20);
void print_vertex_edges(const Graph& graph, Vertex v);
std::pair<Vertex, Vertex> find_connected_pair(const Graph& graph);
