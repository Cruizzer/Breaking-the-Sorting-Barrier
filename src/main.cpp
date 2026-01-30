#include "graph.hpp"
#include "sssp_algorithm.hpp"

#include <iostream>
#include <limits>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " GRAPH_FILE [OPTIONS]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --info [N]              Show graph info (first N vertices, default 20)\n";
        std::cerr << "  --vertex V              Show all edges from vertex V\n";
        std::cerr << "  --find-pair             Find and test a connected vertex pair\n";
        std::cerr << "  START END               Compute shortest distance from START to END\n";
        std::cerr << "  (no options)            Compute SSSP from vertex 0\n";
        return 1;
    }

    std::cout << "Loading graph...\n";
    Graph g = load_osm_graph(argv[1]);
    std::cout << "Graph loaded with " << g.size() << " vertices\n";

    // Handle special modes
    if (argc >= 3 && std::string(argv[2]) == "--info") {
        size_t max_vertices = (argc >= 4) ? std::stoull(argv[3]) : 20;
        print_graph_info(g, max_vertices);
        return 0;
    }
    
    if (argc >= 4 && std::string(argv[2]) == "--vertex") {
        Vertex v = std::stoull(argv[3]);
        print_vertex_edges(g, v);
        return 0;
    }
    
    if (argc >= 3 && std::string(argv[2]) == "--find-pair") {
        auto [start, end] = find_connected_pair(g);
        std::cout << "\nFound connected pair: " << start << " -> " << end << "\n";
        print_vertex_edges(g, start);
        std::cout << "\nTesting shortest path...\n";
        Weight distance = compute_shortest_distance(g, start, end);
        std::cout << "Distance from " << start << " to " << end << ": " 
                  << distance << " meters\n";
        return 0;
    }

    if (argc >= 4) {
        // Point-to-point mode
        Vertex start = std::stoull(argv[2]);
        Vertex end = std::stoull(argv[3]);

        if (start >= g.size() || end >= g.size()) {
            std::cerr << "Error: vertex out of range (max: " << g.size()-1 << ")\n";
            return 1;
        }

        std::cout << "Computing shortest distance from " << start << " to " << end << "...\n";
        Weight distance = compute_shortest_distance(g, start, end);

        if (distance == std::numeric_limits<Weight>::infinity()) {
            std::cout << "No path exists from " << start << " to " << end << "\n";
        } else {
            std::cout << "Shortest distance: " << distance << " meters\n";
        }
    } else {
        // All-pairs mode (from vertex 0)
        std::cout << "Computing SSSP from vertex 0...\n";
        auto distances = compute_sssp(g, 0);

        std::cout << "First 10 distances:\n";
        for (size_t i = 0; i < std::min(distances.size(), size_t(10)); ++i) {
            std::cout << "  dist[" << i << "] = " << distances[i] << "\n";
        }
    }

    std::cout << "Done.\n";
    return 0;
}
