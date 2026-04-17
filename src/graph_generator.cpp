#include "graph_generator.hpp"
#include <algorithm>
#include <set>
#include <unordered_set>

namespace {

void add_vertices_with_random_coords(Graph& g, size_t n, std::mt19937& rng) {
    std::uniform_real_distribution<double> coord_dist(0.0, 100.0);
    for (size_t i = 0; i < n; ++i) {
        g.add_vertex(coord_dist(rng), coord_dist(rng));
    }
}

size_t edge_key(size_t u, size_t v, size_t n) {
    if (u > v) std::swap(u, v);
    return u * n + v;
}

} // namespace

Graph generate_random_graph(size_t n, double avg_degree, double min_weight, double max_weight, unsigned seed) {
    return generate_erdos_renyi_graph(n, avg_degree, min_weight, max_weight, seed);
}

Graph generate_erdos_renyi_graph(size_t n, double avg_degree, double min_weight, double max_weight, unsigned seed) {
    Graph g;
    if (n == 0) return g;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);

    add_vertices_with_random_coords(g, n, rng);

    // G(n, m): choose m unique undirected edges
    size_t target_edges = static_cast<size_t>(n * avg_degree / 2.0);
    std::uniform_int_distribution<size_t> vertex_dist(0, n - 1);
    std::unordered_set<size_t> existing_edges;

    size_t added = 0;
    size_t attempts = 0;
    const size_t max_attempts = std::max<size_t>(target_edges * 20, 1000);

    while (added < target_edges && attempts < max_attempts) {
        attempts++;
        size_t u = vertex_dist(rng);
        size_t v = vertex_dist(rng);

        if (u == v) continue;

        size_t key = edge_key(u, v, n);
        if (existing_edges.count(key)) continue;
        existing_edges.insert(key);

        double w = weight_dist(rng);
        g[u].push_back({v, w, ""});
        g[v].push_back({u, w, ""});
        added++;
    }

    return g;
}

Graph generate_barabasi_albert_graph(size_t n, size_t m_attach, double min_weight, double max_weight, unsigned seed) {
    Graph g;
    if (n == 0) return g;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    add_vertices_with_random_coords(g, n, rng);

    if (n == 1) return g;

    m_attach = std::max<size_t>(1, std::min(m_attach, n - 1));
    const size_t m0 = std::min(n, std::max<size_t>(m_attach + 1, 2));

    std::vector<size_t> degree(n, 0);
    std::vector<size_t> repeated_nodes;

    // Initial clique on m0 vertices
    for (size_t u = 0; u < m0; ++u) {
        for (size_t v = u + 1; v < m0; ++v) {
            double w = weight_dist(rng);
            g[u].push_back({v, w, ""});
            g[v].push_back({u, w, ""});
            degree[u]++;
            degree[v]++;
            repeated_nodes.push_back(u);
            repeated_nodes.push_back(v);
        }
    }

    for (size_t v = m0; v < n; ++v) {
        std::unordered_set<size_t> chosen;

        while (chosen.size() < m_attach) {
            size_t u;
            if (!repeated_nodes.empty()) {
                std::uniform_int_distribution<size_t> pick(0, repeated_nodes.size() - 1);
                u = repeated_nodes[pick(rng)];
            } else {
                std::uniform_int_distribution<size_t> uniform_pick(0, v - 1);
                u = uniform_pick(rng);
            }
            if (u == v) continue;
            chosen.insert(u);
        }

        for (size_t u : chosen) {
            double w = weight_dist(rng);
            g[v].push_back({u, w, ""});
            g[u].push_back({v, w, ""});

            degree[v]++;
            degree[u]++;
            repeated_nodes.push_back(v);
            repeated_nodes.push_back(u);
        }
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
