#include "algorithms/dijkstra.hpp"
#include <queue>
#include <limits>
#include <algorithm>

namespace algorithms {

std::vector<Weight> dijkstra(const Graph& graph, Vertex source) {
    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    dist[source] = 0.0;
    
    // Min-heap priority queue: (distance, vertex)
    using PQElem = std::pair<Weight, Vertex>;
    std::priority_queue<PQElem, std::vector<PQElem>, std::greater<PQElem>> pq;
    pq.push({0.0, source});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        // Skip if we've already found a better path
        if (d > dist[u]) continue;
        
        // Relax all outgoing edges
        for (const auto& edge : graph[u]) {
            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[edge.to]) {
                dist[edge.to] = new_dist;
                pq.push({new_dist, edge.to});
            }
        }
    }
    
    return dist;
}

Weight dijkstra_single_target(const Graph& graph, Vertex source, Vertex target) {
    if (source >= graph.size() || target >= graph.size()) {
        return std::numeric_limits<Weight>::infinity();
    }
    
    if (source == target) {
        return 0.0;
    }
    
    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    dist[source] = 0.0;
    
    using PQElem = std::pair<Weight, Vertex>;
    std::priority_queue<PQElem, std::vector<PQElem>, std::greater<PQElem>> pq;
    pq.push({0.0, source});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        // Early termination: found shortest path to target
        if (u == target) {
            return dist[target];
        }
        
        if (d > dist[u]) continue;
        
        for (const auto& edge : graph[u]) {
            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[edge.to]) {
                dist[edge.to] = new_dist;
                pq.push({new_dist, edge.to});
            }
        }
    }
    
    return dist[target];
}

std::vector<Vertex> dijkstra_path(const Graph& graph, Vertex source, Vertex target) {
    if (source >= graph.size() || target >= graph.size()) {
        return {};
    }
    
    if (source == target) {
        return {source};
    }
    
    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    std::vector<Vertex> parent(n, n);  // n means no parent
    dist[source] = 0.0;
    
    using PQElem = std::pair<Weight, Vertex>;
    std::priority_queue<PQElem, std::vector<PQElem>, std::greater<PQElem>> pq;
    pq.push({0.0, source});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (u == target) {
            break;  // Found target
        }
        
        if (d > dist[u]) continue;
        
        for (const auto& edge : graph[u]) {
            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[edge.to]) {
                dist[edge.to] = new_dist;
                parent[edge.to] = u;
                pq.push({new_dist, edge.to});
            }
        }
    }
    
    // Reconstruct path
    if (dist[target] == std::numeric_limits<Weight>::infinity()) {
        return {};  // No path exists
    }
    
    std::vector<Vertex> path;
    Vertex current = target;
    while (current != source) {
        path.push_back(current);
        current = parent[current];
        if (current == n) {
            return {};  // Path broken
        }
    }
    path.push_back(source);
    std::reverse(path.begin(), path.end());
    
    return path;
}

} // namespace algorithms
