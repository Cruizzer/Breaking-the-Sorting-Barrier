#include "graph.hpp"
#include "sssp_algorithm.hpp"

#include <iostream>
#include <limits>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " GRAPH_FILE [START_VERTEX END_VERTEX]\n";
        std::cerr << "  If START and END are provided, computes shortest distance between them.\n";
        std::cerr << "  Otherwise, computes SSSP from vertex 0 to all vertices.\n";
        return 1;
    }

    std::cout << "Loading graph...\n";
    Graph g = load_osm_graph(argv[1]);
    std::cout << "Graph loaded with " << g.size() << " vertices\n";

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
