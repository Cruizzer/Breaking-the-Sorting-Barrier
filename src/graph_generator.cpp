#include "graph_generator.hpp"
#include <algorithm>
#include <set>

Graph generate_random_graph(size_t n, double avg_degree, double min_weight, double max_weight, unsigned seed) {
    Graph g;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    std::uniform_real_distribution<double> coord_dist(0.0, 100.0);
    
    // Create vertices with random coordinates
    for (size_t i = 0; i < n; ++i) {
        g.add_vertex(coord_dist(rng), coord_dist(rng));
    }
    
    // Add edges to achieve target average degree
    size_t target_edges = static_cast<size_t>(n * avg_degree / 2.0);
    std::uniform_int_distribution<size_t> vertex_dist(0, n - 1);
    std::set<std::pair<size_t, size_t>> existing_edges;
    
    for (size_t i = 0; i < target_edges; ++i) {
        size_t u = vertex_dist(rng);
        size_t v = vertex_dist(rng);
        
        if (u == v) continue; // No self-loops
        if (u > v) std::swap(u, v); // Normalize edge representation
        
        if (existing_edges.count({u, v})) continue; // No duplicate edges
        existing_edges.insert({u, v});
        
        double w = weight_dist(rng);
        g[u].push_back({v, w, ""});
        g[v].push_back({u, w, ""}); // Undirected
    }
    
    return g;
}

Graph generate_grid_graph(size_t rows, size_t cols, double min_weight, double max_weight, unsigned seed) {
    Graph g;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    size_t n = rows * cols;
    
    // Create grid vertices
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            g.add_vertex(static_cast<double>(r), static_cast<double>(c));
        }
    }
    
    // Connect neighbors (4-connected grid)
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            size_t v = r * cols + c;
            
            // Right neighbor
            if (c + 1 < cols) {
                size_t right = r * cols + (c + 1);
                double w = weight_dist(rng);
                g[v].push_back({right, w, ""});
                g[right].push_back({v, w, ""});
            }
            
            // Down neighbor
            if (r + 1 < rows) {
                size_t down = (r + 1) * cols + c;
                double w = weight_dist(rng);
                g[v].push_back({down, w, ""});
                g[down].push_back({v, w, ""});
            }
        }
    }
    
    return g;
}

Graph generate_road_network(size_t n, double min_weight, double max_weight, unsigned seed) {
    // Generate a more realistic road-like structure
    // Main roads with branches (tree-like with some cycles)
    Graph g;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    std::uniform_real_distribution<double> coord_dist(0.0, 100.0);
    std::uniform_int_distribution<size_t> branch_dist(1, 3);
    
    if (n == 0) return g;
    
    // Create root vertex
    g.add_vertex(coord_dist(rng), coord_dist(rng));
    
    // Build tree structure with branches
    std::vector<size_t> frontier = {0};
    
    while (g.size() < n && !frontier.empty()) {
        size_t u = frontier.back();
        frontier.pop_back();
        
        size_t branches = std::min(branch_dist(rng), n - g.size());
        
        for (size_t i = 0; i < branches; ++i) {
            size_t v = g.size();
            g.add_vertex(coord_dist(rng), coord_dist(rng));
            
            double w = weight_dist(rng);
            g[u].push_back({v, w, ""});
            g[v].push_back({u, w, ""});
            
            if (g.size() < n) {
                frontier.push_back(v);
            }
        }
    }
    
    // Add some random cross-edges for cycles (10% of vertices)
    std::uniform_int_distribution<size_t> vertex_dist(0, n - 1);
    size_t cross_edges = n / 10;
    
    for (size_t i = 0; i < cross_edges; ++i) {
        size_t u = vertex_dist(rng);
        size_t v = vertex_dist(rng);
        
        if (u == v) continue;
        
        // Check if edge already exists
        bool exists = false;
        for (const auto& e : g[u]) {
            if (e.to == v) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            double w = weight_dist(rng);
            g[u].push_back({v, w, ""});
            g[v].push_back({u, w, ""});
        }
    }
    
    return g;
}
