#pragma once
#include <vector>
#include <cstddef>
#include <string>
#include <utility>

// Vertex and weight types
using Vertex = std::size_t;
using Weight = double;

// Edge structure
struct Edge {
    Vertex to;
    Weight weight;
    std::string name; // Optional road name (from OSM way)
};

// Graph with adjacency list and coordinates for each vertex
struct Graph {
    std::vector<std::vector<Edge>> adj;
    std::vector<std::pair<double, double>> coords; // {lat, lon}

    std::size_t size() const { return adj.size(); }
    bool empty() const { return adj.empty(); }

    std::vector<Edge>& operator[](std::size_t i) { return adj[i]; }
    const std::vector<Edge>& operator[](std::size_t i) const { return adj[i]; }

    void add_vertex(double lat, double lon) {
        adj.emplace_back();
        coords.emplace_back(lat, lon);
    }
};

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
