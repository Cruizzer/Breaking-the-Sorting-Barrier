// bmssp_duan25.cpp
// Implementation of BMSSP algorithm from Duan et al. (2025)

#include "algorithms/bmssp.hpp"

namespace duan25 {

// ─────────────────────────────────────────────────────────────────────────────
// Constructors
// ─────────────────────────────────────────────────────────────────────────────

Solver::Solver(int n)
    : num_real_vertices_(n)
{
    input_adj_.assign(n, {});
}

Solver::Solver(const AdjList& adj)
    : num_real_vertices_((int)adj.size()), input_adj_(adj)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Public methods
// ─────────────────────────────────────────────────────────────────────────────

void Solver::add_edge(int u, int v, double w) {
    input_adj_[u].push_back({ v, w });
}

void Solver::prepare_graph(bool apply_cd_transform) {
    cd_transform_applied_ = apply_cd_transform;
    remove_parallel_edges();

    if (!apply_cd_transform) {
        build_identity_node_map();
    } else {
        apply_constant_degree_transform();
    }

    allocate_algorithm_state();
}

std::pair<std::vector<double>, std::vector<int>> Solver::execute(int source) {
    reset_state();

    int internal_src = real_to_internal_[source];
    dist_estimate_[internal_src] = 0.0;
    hop_count_[internal_src]     = 0;
    predecessor_[internal_src]   = internal_src;

    int levels = (int)std::ceil(std::log2((double)working_adj_.size()) / t_);

    PathLabel infinite = infinite_label();
    bmssp_rec(levels, infinite, { internal_src });

    return build_output();
}

std::vector<int> Solver::reconstruct_path(int target,
                                          const std::vector<int>& real_pred) const {
    // Check reachability against the internal distance for the target's proxy node.
    int internal_target = real_to_internal_[target];
    if (dist_estimate_[internal_target] >= INF) return {};

    if (!cd_transform_applied_) {
        // Predecessor array is directly in real-vertex space.
        int len = hop_count_[real_to_internal_[target]] + 1;
        std::vector<int> path(len);
        int u = target;
        for (int i = len - 1; i >= 0; --i) {
            path[i] = u;
            u = real_pred[u];
        }
        return path;
    } else {
        // Walk real_pred until we reach the source (self-loop).
        std::vector<int> path;
        int u = target, prev;
        do {
            path.push_back(u);
            prev = u;
            u    = real_pred[u];
        } while (u != prev);
        std::reverse(path.begin(), path.end());
        return path;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Graph preparation helpers
// ─────────────────────────────────────────────────────────────────────────────

void Solver::remove_parallel_edges() {
    // tmp[j] = (last source that considered j, index in new adj list)
    std::vector<std::pair<int,int>> tmp(num_real_vertices_, {-1, -1});
    for (int u = 0; u < num_real_vertices_; ++u) {
        std::vector<Edge> clean;
        for (const Edge& e : input_adj_[u]) {
            int v = e.to;
            if (tmp[v].first != u) {
                tmp[v] = { u, (int)clean.size() };
                clean.push_back(e);
            } else {
                int idx = tmp[v].second;
                clean[idx].weight = std::min(clean[idx].weight, e.weight);
            }
        }
        input_adj_[u] = std::move(clean);
    }
}

void Solver::build_identity_node_map() {
    working_adj_ = std::move(input_adj_);
    int n = (int)working_adj_.size();
    real_to_internal_.resize(n);
    internal_to_real_.resize(n);
    std::iota(real_to_internal_.begin(), real_to_internal_.end(), 0);
    std::iota(internal_to_real_.begin(), internal_to_real_.end(), 0);
}

void Solver::apply_constant_degree_transform() {
    // Assign a unique proxy id to each directed edge in both directions.
    std::vector<std::map<int,int>> edge_proxy(num_real_vertices_);
    int proxy_count = 0;
    for (int u = 0; u < num_real_vertices_; ++u) {
        for (const Edge& e : input_adj_[u]) {
            int v = e.to;
            if (edge_proxy[u].find(v) == edge_proxy[u].end()) {
                edge_proxy[u][v] = proxy_count++;
                edge_proxy[v][u] = proxy_count++;
            }
        }
    }

    int total_internal = proxy_count + 1;
    int sentinel_node  = proxy_count;

    working_adj_.assign(total_internal, {});
    real_to_internal_.resize(total_internal);
    internal_to_real_.resize(total_internal);

    // Build 0-weight cycles for each original vertex's proxy nodes.
    for (int u = 0; u < num_real_vertices_; ++u) {
        auto& proxies = edge_proxy[u];
        for (auto cur = proxies.begin(); cur != proxies.end(); ++cur) {
            auto nxt = std::next(cur);
            if (nxt == proxies.end()) nxt = proxies.begin();
            working_adj_[cur->second].push_back({ nxt->second, 0.0 });
            internal_to_real_[cur->second] = u;
        }
    }

    // Add the original weighted edges as arcs between proxy nodes.
    for (int u = 0; u < num_real_vertices_; ++u) {
        for (const Edge& e : input_adj_[u]) {
            int v = e.to;
            working_adj_[edge_proxy[u][v]].push_back(
                { edge_proxy[v][u], e.weight });
        }
        if (!edge_proxy[u].empty())
            real_to_internal_[u] = edge_proxy[u].begin()->second;
        else
            real_to_internal_[u] = sentinel_node;
    }

    input_adj_.clear();
}

void Solver::allocate_algorithm_state() {
    int n      = (int)working_adj_.size();
    double lgn = std::log2((double)n);

    k_ = (int)std::floor(std::pow(lgn, 1.0 / 3.0));
    t_ = (int)std::floor(std::pow(lgn, 2.0 / 3.0));

    k_ = std::max(k_, 1);
    t_ = std::max(t_, 1);

    dist_estimate_.assign(n, INF);
    hop_count_.assign(n, 0);
    predecessor_.resize(n);
    std::iota(predecessor_.begin(), predecessor_.end(), 0);

    pivot_root_.resize(n);
    tree_size_.resize(n, 0);
    visit_stamp_.assign(n, -1);
    settled_level_.assign(n, -1);

    int max_levels = (int)std::ceil(lgn / t_) + 1;
    level_pqs_.clear();
    for (int i = 0; i < max_levels; ++i)
        level_pqs_.emplace_back(n);
}

void Solver::reset_state() {
    int n = (int)working_adj_.size();
    dist_estimate_.assign(n, INF);
    hop_count_.assign(n, 0);
    std::iota(predecessor_.begin(), predecessor_.end(), 0);
    settled_level_.assign(n, -1);
    visit_stamp_.assign(n, -1);
    pivot_call_id_ = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Distance helpers
// ─────────────────────────────────────────────────────────────────────────────

PathLabel Solver::label_of(int v) const {
    return PathLabel(dist_estimate_[v], hop_count_[v], v, predecessor_[v]);
}

PathLabel Solver::relaxed_label(int u, int v, double w) const {
    return PathLabel(dist_estimate_[u] + w, hop_count_[u] + 1, v, u);
}

void Solver::relax(int u, int v, double w) {
    dist_estimate_[v] = dist_estimate_[u] + w;
    hop_count_[v]     = hop_count_[u] + 1;
    predecessor_[v]   = u;
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm 1: FindPivots
// ─────────────────────────────────────────────────────────────────────────────

std::pair<std::vector<int>, std::vector<int>>
Solver::find_pivots(PathLabel B, const std::vector<int>& S) {
    pivot_call_id_++;

    std::vector<int> W;
    W.reserve(2 * k_ * (int)S.size());

    for (int x : S) {
        W.push_back(x);
        visit_stamp_[x]  = pivot_call_id_;
        pivot_root_[x]   = x;
        tree_size_[x]    = 0;
    }

    std::vector<int> active = S;

    for (int round = 1; round <= k_; ++round) {
        std::vector<int> next_active;
        next_active.reserve(active.size() * 4);

        for (int u : active) {
            for (const Edge& e : working_adj_[u]) {
                int v = e.to;
                PathLabel relaxed = relaxed_label(u, v, e.weight);
                if (relaxed <= label_of(v)) {
                    relax(u, v, e.weight);
                    if (label_of(v) < B) {
                        pivot_root_[v] = pivot_root_[u];
                        next_active.push_back(v);
                    }
                }
            }
        }

        for (int x : next_active) {
            if (visit_stamp_[x] != pivot_call_id_) {
                visit_stamp_[x] = pivot_call_id_;
                W.push_back(x);
            }
        }

        if ((int)W.size() > k_ * (int)S.size()) {
            return { S, W };
        }

        active = std::move(next_active);
    }

    for (int u : W) tree_size_[pivot_root_[u]]++;
    std::vector<int> P;
    P.reserve((int)W.size() / k_ + 1);
    for (int u : S) {
        if (tree_size_[u] >= k_) P.push_back(u);
    }
    for (int u : W) tree_size_[pivot_root_[u]] = 0;

    return { P, W };
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm 2: Base Case (Dijkstra)
// ─────────────────────────────────────────────────────────────────────────────

std::pair<PathLabel, std::vector<int>>
Solver::base_case(PathLabel B, int x) {
    std::vector<int> settled;
    settled.reserve(k_ + 1);

    using HeapEntry = std::pair<PathLabel, int>;
    std::priority_queue<HeapEntry,
                        std::vector<HeapEntry>,
                        std::greater<HeapEntry>> heap;
    heap.push({ label_of(x), x });

    while (!heap.empty() && (int)settled.size() < k_ + 1) {
        auto [lbl, u] = heap.top();
        heap.pop();

        if (lbl > label_of(u)) continue;

        settled.push_back(u);
        for (const Edge& e : working_adj_[u]) {
            int v = e.to;
            PathLabel relaxed = relaxed_label(u, v, e.weight);
            if (relaxed <= label_of(v) && relaxed < B) {
                relax(u, v, e.weight);
                heap.push({ label_of(v), v });
            }
        }
    }

    if ((int)settled.size() <= k_) {
        return { B, settled };
    }

    PathLabel new_bound = label_of(settled.back());
    settled.pop_back();
    return { new_bound, settled };
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm 3: BMSSP (recursive)
// ─────────────────────────────────────────────────────────────────────────────

std::pair<PathLabel, std::vector<int>>
Solver::bmssp_rec(int level, PathLabel B, const std::vector<int>& S) {
    if (level == 0) return base_case(B, S[0]);

    auto [P, W] = find_pivots(B, S);

    int n = (int)working_adj_.size();
    // 2^{(l-1)t} is the block capacity for this level's BatchPQ.  We clamp to
    // n because the structure never holds more than n entries, and the shift
    // would overflow int for large levels.
    int batch_size = (((level - 1) * t_) < 30)
                     ? std::min(1 << ((level - 1) * t_), n)
                     : n;
    BatchPQ& D = level_pqs_[level - 1];
    D.initialise(batch_size, B);

    for (int p : P) D.insert(p, label_of(p));

    PathLabel last_inner_bound = B;
    for (int p : P)
        if (label_of(p) < last_inner_bound)
            last_inner_bound = label_of(p);

    std::vector<int> complete;
    // quota = k * 2^{lt}, clamped to n to avoid overflow.
    long long quota = (long long)k_ * (((level * t_) < 30)
                       ? std::min(1LL << (level * t_), (long long)n)
                       : (long long)n);
    complete.reserve((int)quota + (int)W.size());

    while ((long long)complete.size() < quota && D.size() > 0) {
        auto [sub_bound, mini_S] = D.pull();

        auto [inner_bound, inner_complete] = bmssp_rec(level - 1, sub_bound, mini_S);

        complete.insert(complete.end(),
                        inner_complete.begin(), inner_complete.end());

        std::vector<Entry> to_prepend;
        to_prepend.reserve((int)inner_complete.size() * 4 + (int)mini_S.size());

        for (int u : inner_complete) {
            D.erase(u);
            settled_level_[u] = level;

            for (const Edge& e : working_adj_[u]) {
                int v = e.to;
                PathLabel relaxed = relaxed_label(u, v, e.weight);
                if (relaxed <= label_of(v)) {
                    relax(u, v, e.weight);
                    PathLabel new_lbl = label_of(v);
                    if (sub_bound <= new_lbl && new_lbl < B) {
                        D.insert(v, new_lbl);
                    } else if (inner_bound <= new_lbl && new_lbl < sub_bound) {
                        to_prepend.push_back({ v, new_lbl });
                    }
                }
            }
        }

        for (int x : mini_S) {
            if (inner_bound <= label_of(x))
                to_prepend.push_back({ x, label_of(x) });
        }

        D.batch_prepend(to_prepend);
        last_inner_bound = inner_bound;
    }

    PathLabel return_bound = (D.size() == 0) ? B : last_inner_bound;

    for (int x : W) {
        if (settled_level_[x] != level && label_of(x) < return_bound) {
            complete.push_back(x);
        }
    }

    return { return_bound, complete };
}

// ─────────────────────────────────────────────────────────────────────────────
// Output helpers
// ─────────────────────────────────────────────────────────────────────────────

int Solver::real_predecessor_of(int v) const {
    int real_v = internal_to_real_[v];
    int p = v;
    do {
        p = predecessor_[p];
    } while (internal_to_real_[p] == real_v && predecessor_[p] != p);
    return p;
}

std::pair<std::vector<double>, std::vector<int>> Solver::build_output() const {
    if (!cd_transform_applied_) {
        return { dist_estimate_, predecessor_ };
    }

    std::vector<double> out_dist(num_real_vertices_);
    std::vector<int>    out_pred(num_real_vertices_);
    for (int r = 0; r < num_real_vertices_; ++r) {
        int iv       = real_to_internal_[r];
        out_dist[r]  = dist_estimate_[iv];
        int ip       = real_predecessor_of(iv);
        out_pred[r]  = internal_to_real_[ip];
    }
    return { out_dist, out_pred };
}

} // namespace duan25

// ─────────────────────────────────────────────────────────────────────────────
// Wrapper functions for the algorithms namespace
// ─────────────────────────────────────────────────────────────────────────────

#include "graph.hpp"

namespace algorithms {

std::vector<Weight> bmssp(const Graph& graph, Vertex source) {
    duan25::Solver solver(graph.size());
    
    // Add edges from the graph
    for (std::size_t u = 0; u < graph.size(); ++u) {
        for (const Edge& e : graph[u]) {
            solver.add_edge(u, e.to, e.weight);
        }
    }
    
    // Prepare graph and execute
    solver.prepare_graph(false);
    auto [distances, _] = solver.execute(source);

    // Convert solver sentinel (very large finite) to true infinity so output
    // semantics match Dijkstra and downstream diagnostics.
    const double unreachable_threshold = std::numeric_limits<double>::max() / 20.0;
    for (double& d : distances) {
        if (d >= unreachable_threshold) {
            d = std::numeric_limits<double>::infinity();
        }
    }
    
    return distances;
}

Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target) {
    auto distances = bmssp(graph, source);
    return distances[target];
}

std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target) {
    duan25::Solver solver(graph.size());
    
    // Add edges from the graph
    for (std::size_t u = 0; u < graph.size(); ++u) {
        for (const Edge& e : graph[u]) {
            solver.add_edge(u, e.to, e.weight);
        }
    }
    
    // Prepare graph and execute
    solver.prepare_graph(false);
    auto [distances, predecessors] = solver.execute(source);
    
    // Reconstruct path
    auto path = solver.reconstruct_path(target, predecessors);
    
    // Convert from int to Vertex
    std::vector<Vertex> result;
    for (int v : path) {
        result.push_back(static_cast<Vertex>(v));
    }
    return result;
}

} // namespace algorithms