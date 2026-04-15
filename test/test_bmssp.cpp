#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "graph.hpp"
#include "algorithms/bmssp.hpp"
#include "algorithms/dijkstra.hpp"
#include <limits>
#include <cmath>

using Catch::Approx;

static void require_distances_match(const std::vector<Weight>& a, const std::vector<Weight>& b, double eps = 1e-9) {
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::isinf(a[i]) && std::isinf(b[i])) {
            continue;
        }
        REQUIRE(a[i] == Approx(b[i]).margin(eps));
    }
}

// These tests verify that BMSSP produces the same results as Dijkstra
// Initially they will fail since BMSSP is stubbed out

TEST_CASE("BMSSP: Simple 3-vertex graph", "[bmssp]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);
    
    g.adj[0].push_back({1, 10.0, "edge1"});
    g.adj[1].push_back({2, 20.0, "edge2"});
    g.adj[0].push_back({2, 50.0, "edge3"});
    
    SECTION("All distances match Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_fibonacci = algorithms::dijkstra_fibonacci(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        REQUIRE(dist_bmssp.size() == dist_dijkstra.size());
        require_distances_match(dist_dijkstra, dist_fibonacci);
        for (size_t i = 0; i < dist_dijkstra.size(); ++i) {
            if (std::isinf(dist_dijkstra[i])) {
                REQUIRE(std::isinf(dist_bmssp[i]));
            } else {
                REQUIRE(dist_bmssp[i] == Approx(dist_dijkstra[i]));
            }
        }
    }
    
    SECTION("Single target matches Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra_single_target(g, 0, 2);
        auto dist_bmssp = algorithms::bmssp_single_target(g, 0, 2);
        
        if (std::isinf(dist_dijkstra)) {
            REQUIRE(std::isinf(dist_bmssp));
        } else {
            REQUIRE(dist_bmssp == Approx(dist_dijkstra));
        }
    }
    
    SECTION("Path matches Dijkstra") {
        auto path_d = algorithms::dijkstra_path(g, 0, 2);
        auto path_b = algorithms::bmssp_path(g, 0, 2);
        
        if (path_d.empty()) {
            REQUIRE(path_b.empty());
        } else {
            REQUIRE(path_b.size() == path_d.size());
            // Paths should be identical
            REQUIRE(path_b == path_d);
        }
    }
}

TEST_CASE("BMSSP: Disconnected graph", "[bmssp]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);
    
    g.adj[0].push_back({1, 10.0, "edge1"});
    
    SECTION("Unreachable vertices match Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_fibonacci = algorithms::dijkstra_fibonacci(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        require_distances_match(dist_dijkstra, dist_fibonacci);
        for (size_t i = 0; i < dist_dijkstra.size(); ++i) {
            if (std::isinf(dist_dijkstra[i])) {
                REQUIRE(std::isinf(dist_bmssp[i]));
            } else {
                REQUIRE(dist_bmssp[i] == Approx(dist_dijkstra[i]));
            }
        }
    }
}

TEST_CASE("BMSSP: Diamond graph", "[bmssp]") {
    Graph g;
    for (int i = 0; i < 4; ++i) {
        g.add_vertex(i * 1.0, i * 1.0);
    }
    
    g.adj[0].push_back({1, 5.0, "edge1"});
    g.adj[0].push_back({2, 10.0, "edge2"});
    g.adj[1].push_back({3, 8.0, "edge3"});
    g.adj[2].push_back({3, 2.0, "edge4"});
    
    SECTION("Chooses shortest path like Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_fibonacci = algorithms::dijkstra_fibonacci(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        require_distances_match(dist_dijkstra, dist_fibonacci);
        for (size_t i = 0; i < dist_dijkstra.size(); ++i) {
            REQUIRE(dist_bmssp[i] == Approx(dist_dijkstra[i]));
        }
    }
}

TEST_CASE("BMSSP: Linear chain", "[bmssp]") {
    Graph g;
    int n = 10;
    for (int i = 0; i < n; ++i) {
        g.add_vertex(i * 1.0, 0.0);
    }
    
    for (int i = 0; i < n - 1; ++i) {
        g.adj[i].push_back({i + 1, 1.0, "edge"});
    }
    
    SECTION("Path distances match Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_fibonacci = algorithms::dijkstra_fibonacci(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        require_distances_match(dist_dijkstra, dist_fibonacci);
        for (int i = 0; i < n; ++i) {
            REQUIRE(dist_bmssp[i] == Approx(dist_dijkstra[i]));
        }
    }
}

TEST_CASE("BMSSP: Complete graph", "[bmssp]") {
    Graph g;
    int n = 5;
    for (int i = 0; i < n; ++i) {
        g.add_vertex(i * 1.0, i * 1.0);
    }
    
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) {
                double weight = std::abs(i - j) * 10.0;
                g.adj[i].push_back({j, weight, "edge"});
            }
        }
    }
    
    SECTION("All distances match Dijkstra") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_fibonacci = algorithms::dijkstra_fibonacci(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        require_distances_match(dist_dijkstra, dist_fibonacci);
        for (size_t i = 0; i < dist_dijkstra.size(); ++i) {
            REQUIRE(dist_bmssp[i] == Approx(dist_dijkstra[i]));
        }
    }
}

TEST_CASE("BMSSP: Self-loop handling", "[bmssp]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    
    SECTION("Distance to self is zero") {
        auto dist_dijkstra = algorithms::dijkstra(g, 0);
        auto dist_bmssp = algorithms::bmssp(g, 0);
        
        REQUIRE(dist_bmssp[0] == Approx(0.0));
        REQUIRE(dist_bmssp[0] == Approx(dist_dijkstra[0]));
    }
}
