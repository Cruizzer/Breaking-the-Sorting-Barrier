// bmssp_duan25.cpp
// Implementation of BMSSP algorithm from Duan et al. (2025)

#include "algorithms/bmssp.hpp"

namespace {

algorithms::BMSSPTelemetry g_bmssp_telemetry;

} // namespace

namespace duan25 {

// ─────────────────────────────────────────────────────────────────────────────
// Constructors
// ─────────────────────────────────────────────────────────────────────────────

Solver::Solver(int n)
    : num_real_vertices(n)
{
    input_adj.assign(n, {});
}

Solver::Solver(const AdjList& adj)
    : num_real_vertices((int)adj.size()), input_adj(adj)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Public methods
// ─────────────────────────────────────────────────────────────────────────────

void Solver::add_edge(int u, int v, double w) {
    validate_real_vertex(u);
    validate_real_vertex(v);
    if (!std::isfinite(w) || w < 0.0) {
        throw std::invalid_argument("bmssp: edge weights must be finite and non-negative");
    }
    input_adj[u].push_back({ v, w });
}

void Solver::prepare_graph(bool apply_cd_transform) {
    cd_transform_applied = apply_cd_transform;
    validate_input_graph();
    remove_parallel_edges();

    if (!apply_cd_transform) {
        build_identity_node_map();
    } else {
        apply_constant_degree_transform();
    }

    allocate_algorithm_state();
}

std::pair<std::vector<double>, std::vector<int>> Solver::execute(int source) {
    validate_real_vertex(source);
    reset_state();

    int internal_src = real_to_internal[source];
    dist_estimate[internal_src] = 0.0;
    hop_count[internal_src]     = 0;
    predecessor[internal_src]   = internal_src;

    int levels = (int)std::ceil(std::log2((double)working_adj.size()) / t);

    PathLabel infinite = infinite_label();
    bmssp_rec(levels, infinite, { internal_src });

    return build_output();
}

std::size_t Solver::working_vertex_count() const {
    return working_adj.size();
}

std::size_t Solver::working_edge_count() const {
    std::size_t total = 0;
    for (const auto& adj : working_adj) {
        total += adj.size();
    }
    return total;
}

std::vector<int> Solver::reconstruct_path(int target,
                                          const std::vector<int>& real_pred) const {
    if (target < 0 || target >= num_real_vertices) return {};
    if (real_pred.size() != static_cast<size_t>(num_real_vertices)) return {};

    // Check reachability using the internal distance of the target's proxy node.
    int internal_target = real_to_internal[target];
    if (dist_estimate[internal_target] >= INF) return {};

    if (!cd_transform_applied) {
        // Predecessor array is directly in real-vertex space.
        int len = hop_count[real_to_internal[target]] + 1;
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
    std::vector<std::pair<int,int>> tmp(num_real_vertices, {-1, -1});
    for (int u = 0; u < num_real_vertices; ++u) {
        std::vector<Edge> clean;
        for (const Edge& e : input_adj[u]) {
            int v = e.to;
            if (tmp[v].first != u) {
                tmp[v] = { u, (int)clean.size() };
                clean.push_back(e);
            } else {
                int idx = tmp[v].second;
                clean[idx].weight = std::min(clean[idx].weight, e.weight);
            }
        }
        input_adj[u] = std::move(clean);
    }
}

void Solver::validate_input_graph() const {
    for (int u = 0; u < num_real_vertices; ++u) {
        for (const Edge& e : input_adj[u]) {
            if (e.to < 0 || e.to >= num_real_vertices) {
                throw std::out_of_range("bmssp: edge endpoint is out of range");
            }
            if (!std::isfinite(e.weight) || e.weight < 0.0) {
                throw std::invalid_argument("bmssp: edge weights must be finite and non-negative");
            }
        }
    }
}

void Solver::validate_real_vertex(int v) const {
    if (v < 0 || v >= num_real_vertices) {
        throw std::out_of_range("bmssp: vertex index is out of range");
    }
}

void Solver::build_identity_node_map() {
    working_adj = std::move(input_adj);
    int n = (int)working_adj.size();
    real_to_internal.resize(n);
    internal_to_real.resize(n);
    std::iota(real_to_internal.begin(), real_to_internal.end(), 0);
    std::iota(internal_to_real.begin(), internal_to_real.end(), 0);
}

void Solver::apply_constant_degree_transform() {
    // Assign a unique proxy id to each directed edge in both directions.
    std::vector<std::map<int,int>> edge_proxy(num_real_vertices);
    int proxy_count = 0;
    for (int u = 0; u < num_real_vertices; ++u) {
        for (const Edge& e : input_adj[u]) {
            int v = e.to;
            if (edge_proxy[u].find(v) == edge_proxy[u].end()) {
                edge_proxy[u][v] = proxy_count++;
                edge_proxy[v][u] = proxy_count++;
            }
        }
    }

    int total_internal = proxy_count + 1;
    int sentinel_node  = proxy_count;

    working_adj.assign(total_internal, {});
    real_to_internal.resize(total_internal);
    internal_to_real.resize(total_internal);

    // Build 0-weight cycles for each original vertex's proxy nodes.
    for (int u = 0; u < num_real_vertices; ++u) {
        auto& proxies = edge_proxy[u];
        for (auto cur = proxies.begin(); cur != proxies.end(); ++cur) {
            auto nxt = std::next(cur);
            if (nxt == proxies.end()) nxt = proxies.begin();
            working_adj[cur->second].push_back({ nxt->second, 0.0 });
            internal_to_real[cur->second] = u;
        }
    }

    // Add the original weighted edges as arcs between proxy nodes.
    for (int u = 0; u < num_real_vertices; ++u) {
        for (const Edge& e : input_adj[u]) {
            int v = e.to;
            working_adj[edge_proxy[u][v]].push_back(
                { edge_proxy[v][u], e.weight });
        }
        if (!edge_proxy[u].empty())
            real_to_internal[u] = edge_proxy[u].begin()->second;
        else
            real_to_internal[u] = sentinel_node;
    }

    input_adj.clear();
}

void Solver::allocate_algorithm_state() {
    int n      = (int)working_adj.size();
    double lgn = std::log2((double)n);

    k = (int)std::floor(std::pow(lgn, 1.0 / 3.0));
    t = (int)std::floor(std::pow(lgn, 2.0 / 3.0));

    k = std::max(k, 1);
    t = std::max(t, 1);

    dist_estimate.assign(n, INF);
    hop_count.assign(n, 0);
    predecessor.resize(n);
    std::iota(predecessor.begin(), predecessor.end(), 0);

    pivot_root.resize(n);
    tree_size.resize(n, 0);
    visit_stamp.assign(n, -1);
    settled_level.assign(n, -1);

    int max_levels = (int)std::ceil(lgn / t) + 1;
    level_pqs.clear();
    for (int i = 0; i < max_levels; ++i)
        level_pqs.emplace_back(n);
}

void Solver::reset_state() {
    int n = (int)working_adj.size();
    dist_estimate.assign(n, INF);
    hop_count.assign(n, 0);
    std::iota(predecessor.begin(), predecessor.end(), 0);
    settled_level.assign(n, -1);
    visit_stamp.assign(n, -1);
    pivot_call_id = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Distance helpers
// ─────────────────────────────────────────────────────────────────────────────

PathLabel Solver::label_of(int v) const {
    return PathLabel(dist_estimate[v], hop_count[v], v, predecessor[v]);
}

PathLabel Solver::relaxed_label(int u, int v, double w) const {
    return PathLabel(dist_estimate[u] + w, hop_count[u] + 1, v, u);
}

void Solver::relax(int u, int v, double w) {
    dist_estimate[v] = dist_estimate[u] + w;
    hop_count[v]     = hop_count[u] + 1;
    predecessor[v]   = u;
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm 1: FindPivots
// ─────────────────────────────────────────────────────────────────────────────

std::pair<std::vector<int>, std::vector<int>>
Solver::find_pivots(PathLabel B, const std::vector<int>& S) {
    auto batch = discover_pivots(B, S);
    return { batch.pivots, batch.visited };
}

Solver::PivotBatch Solver::discover_pivots(PathLabel B, const std::vector<int>& S) {
    if (g_bmssp_telemetry.enabled) {
        g_bmssp_telemetry.find_pivots_calls++;
    }

    pivot_call_id++;

    std::vector<int> W;

    for (int x : S) {
        W.push_back(x);
        visit_stamp[x]  = pivot_call_id;
        pivot_root[x]   = x;
        tree_size[x]    = 0;
    }

    std::vector<int> active = S;

    for (int round = 1; round <= k; ++round) {
        std::vector<int> next_active;

        for (int u : active) {
            for (const Edge& e : working_adj[u]) {
                int v = e.to;
                PathLabel relaxed = relaxed_label(u, v, e.weight);
                if (relaxed <= label_of(v)) {
                    relax(u, v, e.weight);
                    if (label_of(v) < B) {
                        pivot_root[v] = pivot_root[u];
                        next_active.push_back(v);
                    }
                }
            }
        }

        for (int x : next_active) {
            if (visit_stamp[x] != pivot_call_id) {
                visit_stamp[x] = pivot_call_id;
                W.push_back(x);
            }
        }

        if ((int)W.size() > k * (int)S.size()) {
            if (g_bmssp_telemetry.enabled && !S.empty()) {
                g_bmssp_telemetry.pivot_ratio_sum += 1.0;
                g_bmssp_telemetry.frontier_expansion_sum +=
                    static_cast<double>(W.size()) / static_cast<double>(S.size());
            }
            return { S, W };
        }

        active = next_active;
    }

    for (int u : W) tree_size[pivot_root[u]]++;
    std::vector<int> P;
    for (int u : S) {
        if (tree_size[u] >= k) P.push_back(u);
    }
    for (int u : W) tree_size[pivot_root[u]] = 0;

    if (g_bmssp_telemetry.enabled && !S.empty()) {
        g_bmssp_telemetry.pivot_ratio_sum +=
            static_cast<double>(P.size()) / static_cast<double>(S.size());
        g_bmssp_telemetry.frontier_expansion_sum +=
            static_cast<double>(W.size()) / static_cast<double>(S.size());
    }

    return { P, W };
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm 2: Base Case (Dijkstra)
// ─────────────────────────────────────────────────────────────────────────────

std::pair<PathLabel, std::vector<int>>
Solver::base_case(PathLabel B, int x) {
    std::vector<int> settled;

    using HeapEntry = std::pair<PathLabel, int>;
    std::priority_queue<HeapEntry,
                        std::vector<HeapEntry>,
                        std::greater<HeapEntry>> heap;
    heap.push({ label_of(x), x });

    while (!heap.empty() && (int)settled.size() < k + 1) {
        auto [lbl, u] = heap.top();
        heap.pop();

        if (lbl > label_of(u)) continue;

        settled.push_back(u);
        for (const Edge& e : working_adj[u]) {
            int v = e.to;
            PathLabel relaxed = relaxed_label(u, v, e.weight);
            if (relaxed <= label_of(v) && relaxed < B) {
                relax(u, v, e.weight);
                heap.push({ label_of(v), v });
            }
        }
    }

    if ((int)settled.size() <= k) {
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
    if (g_bmssp_telemetry.enabled) {
        g_bmssp_telemetry.calls_total++;
        g_bmssp_telemetry.max_level = std::max(g_bmssp_telemetry.max_level, level);
    }

    if (level == 0) return base_case(B, S[0]);

    LevelStep step = process_level(level, B, S);
    return { step.bound, step.completed };
}

Solver::LevelStep Solver::process_level(int level, PathLabel B, const std::vector<int>& S) {
    auto pivots = discover_pivots(B, S);

    const long long batch_size = (1ll << ((level - 1) * t));
    BatchPQ& queue = level_pqs[level - 1];
    queue.initialise(batch_size, B);

    for (int p : pivots.pivots) queue.insert(p, label_of(p));
    if (g_bmssp_telemetry.enabled) {
        g_bmssp_telemetry.queue_insert_count += pivots.pivots.size();
    }

    std::vector<int> completed;
    const long long quota = k * (1ll << (level * t));
    PathLabel frontier_bound = B;

    while ((long long)completed.size() < quota && queue.size() > 0) {
        auto [child_bound, child_seeds] = queue.pull();
        if (g_bmssp_telemetry.enabled) {
            g_bmssp_telemetry.pull_count++;
            g_bmssp_telemetry.pull_total_batch += child_seeds.size();
        }
        auto [next_bound, next_completed] = bmssp_rec(level - 1, child_bound, child_seeds);

        completed.insert(completed.end(), next_completed.begin(), next_completed.end());

        std::vector<Entry> postponed;
        relax_from_completed_vertices(level,
                                      B,
                                      child_bound,
                                      next_bound,
                                      next_completed,
                                      postponed,
                                      queue);

        for (int x : child_seeds) {
            if (next_bound <= label_of(x)) {
                postponed.push_back({ x, label_of(x) });
            }
        }

        queue.batch_prepend(postponed);
        if (g_bmssp_telemetry.enabled) {
            g_bmssp_telemetry.queue_batchprepend_count++;
        }
        frontier_bound = next_bound;
    }

    PathLabel return_bound = (queue.size() == 0) ? B : frontier_bound;
    for (int x : pivots.visited) {
        if (settled_level[x] != level && label_of(x) < return_bound) {
            completed.push_back(x);
        }
    }

    return { return_bound, completed };
}

void Solver::relax_from_completed_vertices(int level,
                                           PathLabel parent_bound,
                                           PathLabel child_pull_bound,
                                           PathLabel child_complete_bound,
                                           const std::vector<int>& completed,
                                           std::vector<Entry>& postponed,
                                           BatchPQ& queue) {
    for (int u : completed) {
        queue.erase(u);
        if (g_bmssp_telemetry.enabled) {
            g_bmssp_telemetry.queue_erase_count++;
        }
        settled_level[u] = level;

        for (const Edge& e : working_adj[u]) {
            int v = e.to;
            PathLabel candidate = relaxed_label(u, v, e.weight);
            if (candidate <= label_of(v)) {
                relax(u, v, e.weight);
                PathLabel updated = label_of(v);
                if (child_pull_bound <= updated && updated < parent_bound) {
                    queue.insert(v, updated);
                    if (g_bmssp_telemetry.enabled) {
                        g_bmssp_telemetry.queue_insert_count++;
                    }
                } else if (child_complete_bound <= updated && updated < child_pull_bound) {
                    postponed.push_back({ v, updated });
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Output helpers
// ─────────────────────────────────────────────────────────────────────────────

int Solver::real_predecessor_of(int v) const {
    int real_v = internal_to_real[v];
    int p = v;
    do {
        p = predecessor[p];
    } while (internal_to_real[p] == real_v && predecessor[p] != p);
    return p;
}

std::pair<std::vector<double>, std::vector<int>> Solver::build_output() const {
    if (!cd_transform_applied) {
        return { dist_estimate, predecessor };
    }

    std::vector<double> out_dist(num_real_vertices);
    std::vector<int>    out_pred(num_real_vertices);
    for (int r = 0; r < num_real_vertices; ++r) {
        int iv       = real_to_internal[r];
        out_dist[r]  = dist_estimate[iv];
        int ip       = real_predecessor_of(iv);
        out_pred[r]  = internal_to_real[ip];
    }
    return { out_dist, out_pred };
}

} // namespace duan25

// ─────────────────────────────────────────────────────────────────────────────
// Wrapper functions for the algorithms namespace
// ─────────────────────────────────────────────────────────────────────────────

#include "graph.hpp"

namespace algorithms {

void set_bmssp_telemetry_enabled(bool enabled) {
    g_bmssp_telemetry.enabled = enabled;
}

void reset_bmssp_telemetry() {
    const bool enabled = g_bmssp_telemetry.enabled;
    g_bmssp_telemetry = BMSSPTelemetry{};
    g_bmssp_telemetry.enabled = enabled;
}

BMSSPTelemetry get_bmssp_telemetry() {
    return g_bmssp_telemetry;
}

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
    auto [distances, ignored_predecessors] = solver.execute(source);

    // Convert the solver sentinel (very large finite value) to true infinity
    // so output semantics match Dijkstra and downstream diagnostics.
    const double unreachable_threshold = std::numeric_limits<double>::max() / 20.0;
    for (double& d : distances) {
        if (d >= unreachable_threshold) {
            d = std::numeric_limits<double>::infinity();
        }
    }
    
    return distances;
}

Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target) {
    if (target >= graph.size()) {
        return std::numeric_limits<Weight>::infinity();
    }
    auto distances = bmssp(graph, source);
    return distances[target];
}

std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target) {
    if (target >= graph.size()) {
        return {};
    }

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
    
    // Convert int path entries to Vertex
    std::vector<Vertex> result;
    for (int v : path) {
        result.push_back(static_cast<Vertex>(v));
    }
    return result;
}

} // namespace algorithms