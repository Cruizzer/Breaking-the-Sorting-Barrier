#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "graph.hpp"

using Catch::Approx;

TEST_CASE("Graph: Basic vertex operations", "[graph]") {
    Graph g;
    
    SECTION("Add vertices") {
        g.add_vertex(0.0, 0.0);
        g.add_vertex(1.0, 1.0);
        g.add_vertex(2.0, 2.0);
        
        REQUIRE(g.adj.size() == 3);
        REQUIRE(g.coords.size() == 3);
        REQUIRE(g.coords[0].first == Approx(0.0));
        REQUIRE(g.coords[0].second == Approx(0.0));
        REQUIRE(g.coords[2].first == Approx(2.0));
        REQUIRE(g.coords[2].second == Approx(2.0));
    }
    
    SECTION("Empty adjacency lists initially") {
        g.add_vertex(0.0, 0.0);
        REQUIRE(g.adj[0].empty());
    }
}

TEST_CASE("Graph: Edge operations", "[graph]") {
    Graph g;
    g.add_vertex(0.0, 0.0);
    g.add_vertex(1.0, 1.0);
    
    SECTION("Add edge") {
        g.adj[0].push_back({1, 10.5, "test_edge"});
        
        REQUIRE(g.adj[0].size() == 1);
        REQUIRE(g.adj[0][0].to == 1);
        REQUIRE(g.adj[0][0].weight == Approx(10.5));
        REQUIRE(g.adj[0][0].name == "test_edge");
    }
    
    SECTION("Multiple edges from same vertex") {
        g.add_vertex(2.0, 2.0);
        
        g.adj[0].push_back({1, 10.0, "edge1"});
        g.adj[0].push_back({2, 20.0, "edge2"});
        
        REQUIRE(g.adj[0].size() == 2);
        REQUIRE(g.adj[0][0].to == 1);
        REQUIRE(g.adj[0][1].to == 2);
    }
}

TEST_CASE("Graph: Operator[] access", "[graph]") {
    Graph g;
    
    SECTION("Const access") {
        g.add_vertex(0.0, 0.0);
        const Graph& cg = g;
        
        REQUIRE(cg[0].empty());
    }
    
    SECTION("Non-const access") {
        g.add_vertex(0.0, 0.0);
        g.add_vertex(1.0, 1.0);
        
        g[0].push_back({1, 5.0, "edge"});
        REQUIRE(g[0].size() == 1);
    }
}

TEST_CASE("Graph: Empty graph", "[graph]") {
    Graph g;
    
    REQUIRE(g.adj.empty());
    REQUIRE(g.coords.empty());
}

TEST_CASE("Graph: Large graph structure", "[graph]") {
    Graph g;
    int n = 1000;
    
    SECTION("Can handle many vertices") {
        for (int i = 0; i < n; ++i) {
            g.add_vertex(i * 0.1, i * 0.2);
        }
        
        REQUIRE(g.adj.size() == n);
        REQUIRE(g.coords.size() == n);
    }
    
    SECTION("Can handle many edges") {
        for (int i = 0; i < n; ++i) {
            g.add_vertex(i * 0.1, i * 0.2);
        }
        
        // Add edges to form a chain
        for (int i = 0; i < n - 1; ++i) {
            g.adj[i].push_back({static_cast<Vertex>(i + 1), 1.0, "edge"});
        }
        
        REQUIRE(g.adj[0].size() == 1);
        REQUIRE(g.adj[n - 1].empty());
    }
}
