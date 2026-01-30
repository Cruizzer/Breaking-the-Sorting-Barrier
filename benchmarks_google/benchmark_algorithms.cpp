#include <benchmark/benchmark.h>
#include "graph.hpp"
#include "graph_generator.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/bmssp.hpp"
#include <random>

// ==================== Random Graph Benchmarks ====================

static void BM_Dijkstra_Random_Small(benchmark::State& state) {
    int n = state.range(0);
    int avg_degree = state.range(1);
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
    state.counters["vertices"] = n;
    state.counters["avg_degree"] = avg_degree;
}
BENCHMARK(BM_Dijkstra_Random_Small)
    ->Args({1000, 4})
    ->Args({5000, 4})
    ->Args({10000, 4})
    ->Args({1000, 8})
    ->Args({5000, 8})
    ->Args({10000, 8})
    ->Complexity();

static void BM_BMSSP_Random_Small(benchmark::State& state) {
    int n = state.range(0);
    int avg_degree = state.range(1);
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
    state.counters["vertices"] = n;
    state.counters["avg_degree"] = avg_degree;
}
BENCHMARK(BM_BMSSP_Random_Small)
    ->Args({1000, 4})
    ->Args({5000, 4})
    ->Args({10000, 4})
    ->Args({1000, 8})
    ->Args({5000, 8})
    ->Args({10000, 8})
    ->Complexity();

// Large graphs
static void BM_Dijkstra_Random_Large(benchmark::State& state) {
    int n = state.range(0);
    int avg_degree = 4;
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_Dijkstra_Random_Large)
    ->Arg(50000)
    ->Arg(100000)
    ->Arg(200000)
    ->Complexity();

static void BM_BMSSP_Random_Large(benchmark::State& state) {
    int n = state.range(0);
    int avg_degree = 4;
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_BMSSP_Random_Large)
    ->Arg(50000)
    ->Arg(100000)
    ->Arg(200000)
    ->Complexity();

// ==================== Grid Graph Benchmarks ====================

static void BM_Dijkstra_Grid(benchmark::State& state) {
    int side = state.range(0);
    
    Graph g = generate_grid_graph(side, side);
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(side * side);
    state.counters["grid_size"] = side;
}
BENCHMARK(BM_Dijkstra_Grid)
    ->Arg(50)
    ->Arg(100)
    ->Arg(200)
    ->Arg(316)  // ~100K vertices
    ->Complexity();

static void BM_BMSSP_Grid(benchmark::State& state) {
    int side = state.range(0);
    
    Graph g = generate_grid_graph(side, side);
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(side * side);
    state.counters["grid_size"] = side;
}
BENCHMARK(BM_BMSSP_Grid)
    ->Arg(50)
    ->Arg(100)
    ->Arg(200)
    ->Arg(316)  // ~100K vertices
    ->Complexity();

// ==================== Single Target Benchmarks ====================

static void BM_Dijkstra_SingleTarget(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_random_graph(n, 4);
    int target = n - 1;  // Far target
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra_single_target(g, 0, target);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_Dijkstra_SingleTarget)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

static void BM_BMSSP_SingleTarget(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_random_graph(n, 4);
    int target = n - 1;  // Far target
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp_single_target(g, 0, target);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_BMSSP_SingleTarget)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

// ==================== Path Reconstruction Benchmarks ====================

static void BM_Dijkstra_Path(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_random_graph(n, 4);
    int target = n - 1;
    
    for (auto _ : state) {
        auto result = algorithms::dijkstra_path(g, 0, target);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_Dijkstra_Path)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

static void BM_BMSSP_Path(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_random_graph(n, 4);
    int target = n - 1;
    
    for (auto _ : state) {
        auto result = algorithms::bmssp_path(g, 0, target);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_BMSSP_Path)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

// ==================== Varying Density Benchmarks ====================

static void BM_Dijkstra_VaryingDensity(benchmark::State& state) {
    int n = 10000;
    int avg_degree = state.range(0);
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.counters["avg_degree"] = avg_degree;
}
BENCHMARK(BM_Dijkstra_VaryingDensity)
    ->DenseRange(2, 16, 2);  // degrees: 2, 4, 6, 8, 10, 12, 14, 16

static void BM_BMSSP_VaryingDensity(benchmark::State& state) {
    int n = 10000;
    int avg_degree = state.range(0);
    
    Graph g = generate_random_graph(n, avg_degree);
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.counters["avg_degree"] = avg_degree;
}
BENCHMARK(BM_BMSSP_VaryingDensity)
    ->DenseRange(2, 16, 2);  // degrees: 2, 4, 6, 8, 10, 12, 14, 16

// ==================== Road Network Benchmarks ====================

static void BM_Dijkstra_RoadNetwork(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_road_network(n);
    
    for (auto _ : state) {
        auto dist = algorithms::dijkstra(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_Dijkstra_RoadNetwork)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

static void BM_BMSSP_RoadNetwork(benchmark::State& state) {
    int n = state.range(0);
    
    Graph g = generate_road_network(n);
    
    for (auto _ : state) {
        auto dist = algorithms::bmssp(g, 0);
        benchmark::DoNotOptimize(dist);
    }
    
    state.SetComplexityN(n);
}
BENCHMARK(BM_BMSSP_RoadNetwork)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000)
    ->Complexity();

BENCHMARK_MAIN();
