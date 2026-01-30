#include "graph.hpp"
#include "sssp_algorithm.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " GRAPH_FILE\n";
        return 1;
    }
    Graph g = load_graph(argv[1]);
    auto distances = compute_sssp(g, 0);  // source = 0
    for (size_t i = 0; i < distances.size(); ++i) {
        std::cout << "dist[" << i << "] = " << distances[i] << "\n";
    }
    return 0;
}
