#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "graph.hpp"
#include "algorithms/dijkstra.hpp"
#include <limits>
#include <algorithm>
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

TEST_CASE("Dijkstra: Simple 3-vertex graph", "[dijkstra]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);
    
    // 0 -> 1 (weight 10)
    // 1 -> 2 (weight 20)
    // 0 -> 2 (weight 50)
    g.adj[0].push_back({1, 10.0, "edge1"});
    g.adj[1].push_back({2, 20.0, "edge2"});
    g.adj[0].push_back({2, 50.0, "edge3"});
    
    SECTION("All distances from vertex 0") {
        auto dist = algorithms::dijkstra(g, 0);
        
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(10.0));
        REQUIRE(dist[2] == Approx(30.0));  // 0->1->2 is shorter than 0->2
    }
    
    SECTION("Single target from vertex 0 to 2") {
        auto dist = algorithms::dijkstra_single_target(g, 0, 2);
        REQUIRE(dist == Approx(30.0));
    }
    
    SECTION("Path reconstruction from 0 to 2") {
        auto path = algorithms::dijkstra_path(g, 0, 2);
        
        REQUIRE(path.size() == 3);
        REQUIRE(path[0] == 0);
        REQUIRE(path[1] == 1);
        REQUIRE(path[2] == 2);
    }
}

TEST_CASE("Dijkstra Fibonacci: Simple 3-vertex graph", "[dijkstra][fibonacci]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);

    g.adj[0].push_back({1, 10.0, "edge1"});
    g.adj[1].push_back({2, 20.0, "edge2"});
    g.adj[0].push_back({2, 50.0, "edge3"});

    SECTION("All distances from vertex 0") {
        auto dist = algorithms::dijkstra_fibonacci(g, 0);
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(10.0));
        REQUIRE(dist[2] == Approx(30.0));
    }

    SECTION("Single target from vertex 0 to 2") {
        auto dist = algorithms::dijkstra_fibonacci_single_target(g, 0, 2);
        REQUIRE(dist == Approx(30.0));
    }

    SECTION("Path reconstruction from 0 to 2") {
        auto path = algorithms::dijkstra_fibonacci_path(g, 0, 2);
        REQUIRE(path.size() == 3);
        REQUIRE(path[0] == 0);
        REQUIRE(path[1] == 1);
        REQUIRE(path[2] == 2);
    }
}

TEST_CASE("Dijkstra Fibonacci: Disconnected graph", "[dijkstra][fibonacci]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);

    g.adj[0].push_back({1, 10.0, "edge1"});

    SECTION("Unreachable vertex has infinite distance") {
        auto dist = algorithms::dijkstra_fibonacci(g, 0);
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(10.0));
        REQUIRE(dist[2] == std::numeric_limits<double>::infinity());
    }

    SECTION("Single target to unreachable vertex") {
        auto dist = algorithms::dijkstra_fibonacci_single_target(g, 0, 2);
        REQUIRE(dist == std::numeric_limits<double>::infinity());
    }

    SECTION("Path to unreachable vertex is empty") {
        auto path = algorithms::dijkstra_fibonacci_path(g, 0, 2);
        REQUIRE(path.empty());
    }
}

TEST_CASE("Dijkstra Fibonacci matches binary Dijkstra", "[dijkstra][fibonacci]") {
    Graph g;
    for (int i = 0; i < 7; ++i) {
        g.add_vertex(static_cast<double>(i), 0.0);
    }

    g.adj[0].push_back({1, 2.0, "a"});
    g.adj[0].push_back({2, 7.0, "b"});
    g.adj[1].push_back({2, 1.0, "c"});
    g.adj[1].push_back({3, 3.0, "d"});
    g.adj[2].push_back({3, 1.0, "e"});
    g.adj[3].push_back({4, 2.5, "f"});
    g.adj[2].push_back({5, 10.0, "g"});
    g.adj[4].push_back({5, 1.0, "h"});

    auto binary_dist = algorithms::dijkstra(g, 0);
    auto fib_dist = algorithms::dijkstra_fibonacci(g, 0);
    require_distances_match(binary_dist, fib_dist);

    auto binary_target = algorithms::dijkstra_single_target(g, 0, 5);
    auto fib_target = algorithms::dijkstra_fibonacci_single_target(g, 0, 5);
    REQUIRE(fib_target == Approx(binary_target).margin(1e-9));

    auto fib_path = algorithms::dijkstra_fibonacci_path(g, 0, 5);
    REQUIRE(!fib_path.empty());
    REQUIRE(fib_path.front() == 0);
    REQUIRE(fib_path.back() == 5);
}

TEST_CASE("Dijkstra: Disconnected graph", "[dijkstra]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    g.add_vertex(2.0, 2.0);
    
    // 0 -> 1 (weight 10)
    // Vertex 2 is disconnected
    g.adj[0].push_back({1, 10.0, "edge1"});
    
    SECTION("Unreachable vertex has infinite distance") {
        auto dist = algorithms::dijkstra(g, 0);
        
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(10.0));
        REQUIRE(dist[2] == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Single target to unreachable vertex") {
        auto dist = algorithms::dijkstra_single_target(g, 0, 2);
        REQUIRE(dist == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Path to unreachable vertex is empty") {
        auto path = algorithms::dijkstra_path(g, 0, 2);
        
        REQUIRE(path.empty());
    }
}

TEST_CASE("Dijkstra: Single vertex graph", "[dijkstra]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    
    SECTION("Distance to self is zero") {
        auto dist = algorithms::dijkstra(g, 0);
        REQUIRE(dist[0] == Approx(0.0));
    }
    
    SECTION("Path to self is just source") {
        auto path = algorithms::dijkstra_path(g, 0, 0);
        
        REQUIRE(path.size() == 1);
        REQUIRE(path[0] == 0);
    }
}

TEST_CASE("Dijkstra: Diamond graph with multiple paths", "[dijkstra]") {
    Graph g;
    for (int i = 0; i < 4; ++i) {
        g.add_vertex(i * 1.0, i * 1.0);
    }
    
    // Diamond: 0 -> 1 -> 3, 0 -> 2 -> 3
    g.adj[0].push_back({1, 5.0, "edge1"});
    g.adj[0].push_back({2, 10.0, "edge2"});
    g.adj[1].push_back({3, 8.0, "edge3"});
    g.adj[2].push_back({3, 2.0, "edge4"});
    
    SECTION("Shortest path is via vertex 2") {
        auto dist = algorithms::dijkstra(g, 0);
        
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(5.0));
        REQUIRE(dist[2] == Approx(10.0));
        REQUIRE(dist[3] == Approx(12.0));  // 0->2->3 is 12, 0->1->3 is 13
    }
    
    SECTION("Path takes the shorter route") {
        auto path = algorithms::dijkstra_path(g, 0, 3);
        
        REQUIRE(path.size() == 3);
        REQUIRE(path[0] == 0);
        REQUIRE(path[1] == 2);
        REQUIRE(path[2] == 3);
    }
}

TEST_CASE("Dijkstra: Complete graph", "[dijkstra]") {
    Graph g;
    int n = 5;
    for (int i = 0; i < n; ++i) {
        g.add_vertex(i * 1.0, i * 1.0);
    }
    
    // All vertices connected with weight = |i-j|
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) {
                double weight = std::abs(i - j) * 10.0;
                g.adj[i].push_back({j, weight, "edge"});
            }
        }
    }
    
    SECTION("All vertices reachable from 0") {
        auto dist = algorithms::dijkstra(g, 0);
        
        REQUIRE(dist[0] == Approx(0.0));
        REQUIRE(dist[1] == Approx(10.0));
        REQUIRE(dist[2] == Approx(20.0));
        REQUIRE(dist[3] == Approx(30.0));
        REQUIRE(dist[4] == Approx(40.0));
    }
}

TEST_CASE("Dijkstra: Linear chain", "[dijkstra]") {
    Graph g;
    int n = 10;
    for (int i = 0; i < n; ++i) {
        g.add_vertex(i * 1.0, 0.0);
    }
    
    // Chain: 0 -> 1 -> 2 -> ... -> 9
    for (int i = 0; i < n - 1; ++i) {
        g.adj[i].push_back({i + 1, 1.0, "edge"});
    }
    
    SECTION("Distance increases linearly") {
        auto dist = algorithms::dijkstra(g, 0);
        
        for (int i = 0; i < n; ++i) {
            REQUIRE(dist[i] == Approx(i * 1.0));
        }
    }
    
    SECTION("Path is complete chain") {
        auto path = algorithms::dijkstra_path(g, 0, n - 1);
        
        REQUIRE(path.size() == n);
        for (int i = 0; i < n; ++i) {
            REQUIRE(path[i] == i);
        }
    }
}
