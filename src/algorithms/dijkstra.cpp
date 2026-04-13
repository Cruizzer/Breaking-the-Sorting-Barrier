#include "algorithms/dijkstra.hpp"
#include <queue>
#include <limits>
#include <algorithm>
#include <memory>

namespace algorithms {

namespace {

class FibonacciHeap {
public:
    struct Node {
        Vertex vertex;
        Weight key;
        int degree = 0;
        bool mark = false;
        Node* parent = nullptr;
        Node* child = nullptr;
        Node* left = nullptr;
        Node* right = nullptr;

        Node(Vertex v, Weight k) : vertex(v), key(k), left(this), right(this) {}
    };

    Node* insert(Vertex vertex, Weight key) {
        auto owned = std::make_unique<Node>(vertex, key);
        Node* node = owned.get();
        storage_.push_back(std::move(owned));

        if (!min_) {
            min_ = node;
        } else {
            insert_into_root_list(node);
            if (node->key < min_->key) {
                min_ = node;
            }
        }

        ++size_;
        return node;
    }

    bool empty() const {
        return min_ == nullptr;
    }

    std::pair<Vertex, Weight> extract_min() {
        Node* z = min_;

        if (z->child) {
            std::vector<Node*> children;
            Node* child = z->child;
            do {
                children.push_back(child);
                child = child->right;
            } while (child != z->child);

            for (Node* x : children) {
                remove_from_list(x);
                x->parent = nullptr;
                x->mark = false;
                insert_into_root_list(x);
            }
            z->child = nullptr;
        }

        if (z->right == z) {
            min_ = nullptr;
        } else {
            Node* next_root = z->right;
            remove_from_list(z);
            min_ = next_root;
            consolidate();
        }

        --size_;
        return {z->vertex, z->key};
    }

    void decrease_key(Node* x, Weight new_key) {
        if (!x || new_key > x->key) {
            return;
        }

        x->key = new_key;
        Node* y = x->parent;

        if (y && x->key < y->key) {
            cut(x, y);
            cascading_cut(y);
        }

        if (x->key < min_->key) {
            min_ = x;
        }
    }

private:
    Node* min_ = nullptr;
    size_t size_ = 0;
    std::vector<std::unique_ptr<Node>> storage_;

    static void remove_from_list(Node* node) {
        node->left->right = node->right;
        node->right->left = node->left;
        node->left = node;
        node->right = node;
    }

    void insert_into_root_list(Node* node) {
        node->left = min_;
        node->right = min_->right;
        min_->right->left = node;
        min_->right = node;
    }

    void link(Node* y, Node* x) {
        remove_from_list(y);
        y->parent = x;
        y->mark = false;

        if (!x->child) {
            x->child = y;
        } else {
            y->left = x->child;
            y->right = x->child->right;
            x->child->right->left = y;
            x->child->right = y;
        }

        ++x->degree;
    }

    void consolidate() {
        std::vector<Node*> roots;
        Node* start = min_;
        Node* w = start;
        do {
            roots.push_back(w);
            w = w->right;
        } while (w != start);

        std::vector<Node*> degree_table(16, nullptr);

        for (Node* root : roots) {
            if (root->parent != nullptr) {
                continue;
            }
            Node* x = root;
            size_t degree = static_cast<size_t>(x->degree);
            while (true) {
                if (degree >= degree_table.size()) {
                    degree_table.resize(degree + 1, nullptr);
                }

                Node* y = degree_table[degree];
                if (!y) {
                    degree_table[degree] = x;
                    break;
                }

                if (y->key < x->key) {
                    std::swap(x, y);
                }

                degree_table[degree] = nullptr;
                link(y, x);
                degree = static_cast<size_t>(x->degree);
            }
        }

        min_ = nullptr;
        for (Node* node : degree_table) {
            if (!node) {
                continue;
            }
            if (!min_) {
                node->left = node;
                node->right = node;
                min_ = node;
            } else {
                insert_into_root_list(node);
                if (node->key < min_->key) {
                    min_ = node;
                }
            }
        }
    }

    void cut(Node* x, Node* y) {
        if (y->child == x) {
            if (x->right == x) {
                y->child = nullptr;
            } else {
                y->child = x->right;
            }
        }

        remove_from_list(x);
        --y->degree;

        x->parent = nullptr;
        x->mark = false;
        insert_into_root_list(x);
    }

    void cascading_cut(Node* y) {
        Node* z = y->parent;
        if (!z) {
            return;
        }

        if (!y->mark) {
            y->mark = true;
            return;
        }

        cut(y, z);
        cascading_cut(z);
    }
};

} // namespace

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

std::vector<Weight> dijkstra_fibonacci(const Graph& graph, Vertex source) {
    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    if (source >= n) {
        return dist;
    }

    std::vector<bool> settled(n, false);
    std::vector<FibonacciHeap::Node*> handles(n, nullptr);
    FibonacciHeap heap;

    dist[source] = 0.0;
    handles[source] = heap.insert(source, 0.0);

    while (!heap.empty()) {
        auto [u, key] = heap.extract_min();
        handles[u] = nullptr;
        if (settled[u]) {
            continue;
        }
        settled[u] = true;

        if (key > dist[u]) {
            continue;
        }

        for (const auto& edge : graph[u]) {
            Vertex v = edge.to;
            if (settled[v]) {
                continue;
            }

            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                if (!handles[v]) {
                    handles[v] = heap.insert(v, new_dist);
                } else {
                    heap.decrease_key(handles[v], new_dist);
                }
            }
        }
    }

    return dist;
}

Weight dijkstra_fibonacci_single_target(const Graph& graph, Vertex source, Vertex target) {
    if (source >= graph.size() || target >= graph.size()) {
        return std::numeric_limits<Weight>::infinity();
    }

    if (source == target) {
        return 0.0;
    }

    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    std::vector<bool> settled(n, false);
    std::vector<FibonacciHeap::Node*> handles(n, nullptr);
    FibonacciHeap heap;

    dist[source] = 0.0;
    handles[source] = heap.insert(source, 0.0);

    while (!heap.empty()) {
        auto [u, key] = heap.extract_min();
        handles[u] = nullptr;
        if (settled[u]) {
            continue;
        }
        settled[u] = true;

        if (u == target) {
            return dist[target];
        }

        if (key > dist[u]) {
            continue;
        }

        for (const auto& edge : graph[u]) {
            Vertex v = edge.to;
            if (settled[v]) {
                continue;
            }

            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                if (!handles[v]) {
                    handles[v] = heap.insert(v, new_dist);
                } else {
                    heap.decrease_key(handles[v], new_dist);
                }
            }
        }
    }

    return dist[target];
}

std::vector<Vertex> dijkstra_fibonacci_path(const Graph& graph, Vertex source, Vertex target) {
    if (source >= graph.size() || target >= graph.size()) {
        return {};
    }

    if (source == target) {
        return {source};
    }

    const size_t n = graph.size();
    std::vector<Weight> dist(n, std::numeric_limits<Weight>::infinity());
    std::vector<Vertex> parent(n, n);
    std::vector<bool> settled(n, false);
    std::vector<FibonacciHeap::Node*> handles(n, nullptr);
    FibonacciHeap heap;

    dist[source] = 0.0;
    handles[source] = heap.insert(source, 0.0);

    while (!heap.empty()) {
        auto [u, key] = heap.extract_min();
        handles[u] = nullptr;
        if (settled[u]) {
            continue;
        }
        settled[u] = true;

        if (u == target) {
            break;
        }

        if (key > dist[u]) {
            continue;
        }

        for (const auto& edge : graph[u]) {
            Vertex v = edge.to;
            if (settled[v]) {
                continue;
            }

            Weight new_dist = dist[u] + edge.weight;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                parent[v] = u;
                if (!handles[v]) {
                    handles[v] = heap.insert(v, new_dist);
                } else {
                    heap.decrease_key(handles[v], new_dist);
                }
            }
        }
    }

    if (dist[target] == std::numeric_limits<Weight>::infinity()) {
        return {};
    }

    std::vector<Vertex> path;
    Vertex current = target;
    while (current != source) {
        path.push_back(current);
        current = parent[current];
        if (current == n) {
            return {};
        }
    }
    path.push_back(source);
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace algorithms
