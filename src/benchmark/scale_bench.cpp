#include "graph_generator.hpp"
#include "benchmark/benchmark.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/bmssp.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

int main(int argc, char** argv) {
    std::vector<size_t> sizes = {1000, 5000, 10000, 50000};
    std::vector<int> degrees = {4, 8};
    size_t trials = 3;

    std::ofstream out("scale_results.csv");
    out << "graph_type,size,param,trials,dijkstra_ms,dijkstra_fib_ms,bmssp_ms,bmssp_vs_binary_speedup,bmssp_vs_fib_speedup,reachable,avg_distance,max_distance\n";

    auto run_one = [&](const std::string& type, size_t N, int param){
        double total_d_ms = 0.0;
        double total_fib_ms = 0.0;
        double total_b_ms = 0.0;
        double total_speedup_binary = 0.0;
        double total_speedup_fib = 0.0;
        size_t total_reachable = 0;
        double total_avgdist = 0.0;
        double total_maxdist = 0.0;

        for (size_t t = 0; t < trials; ++t) {
            Graph g;
            if (type == "random") {
                g = generate_erdos_renyi_graph(N, param);
            } else if (type == "erdos_renyi") {
                g = generate_erdos_renyi_graph(N, param);
            } else if (type == "barabasi_albert") {
                size_t m_attach = static_cast<size_t>(std::max(1, param));
                g = generate_barabasi_albert_graph(N, m_attach);
            } else if (type == "grid") {
                size_t side = std::max<size_t>(1, (size_t)std::sqrt((double)N));
                g = generate_grid_graph(side, side);
            } else if (type == "road") {
                g = generate_road_network(N);
            }

            Vertex source = (t * 1234567) % g.size();

            auto comp = benchmark::compare_algorithms(
                algorithms::dijkstra,
                algorithms::bmssp,
                g,
                source
            );

            auto fib = benchmark::run_benchmark(
                "DijkstraFib",
                algorithms::dijkstra_fibonacci,
                g,
                source
            );

            total_d_ms += comp.dijkstra_result.execution_time_ms;
            total_fib_ms += fib.execution_time_ms;
            total_b_ms += comp.bmssp_result.execution_time_ms;
            total_speedup_binary += comp.speedup_factor;
            if (comp.bmssp_result.execution_time_us > 0.0) {
                total_speedup_fib += fib.execution_time_us / comp.bmssp_result.execution_time_us;
            }
            total_reachable += comp.dijkstra_result.reachable_vertices;
            total_avgdist += comp.dijkstra_result.avg_distance;
            total_maxdist += comp.dijkstra_result.max_distance;
        }

        double avg_d = total_d_ms / trials;
        double avg_fib = total_fib_ms / trials;
        double avg_b = total_b_ms / trials;
        double avg_speed_binary = total_speedup_binary / trials;
        double avg_speed_fib = total_speedup_fib / trials;
        double avg_reachable = static_cast<double>(total_reachable) / trials;
        double avg_dist = total_avgdist / trials;
        double avg_max = total_maxdist / trials;

        out << type << "," << N << "," << param << "," << trials << ","
            << avg_d << "," << avg_fib << "," << avg_b << ","
            << avg_speed_binary << "," << avg_speed_fib << ","
            << avg_reachable << "," << avg_dist << "," << avg_max << "\n";

        std::cout << "Completed: " << type << " N=" << N << " param=" << param
                  << " -> d=" << avg_d << " ms, fib=" << avg_fib << " ms, b=" << avg_b << " ms\n";
    };

    // Random graphs
    for (auto N : sizes) {
        for (int deg : degrees) run_one("random", N, deg);
    }

    // Barabasi-Albert graphs
    std::vector<int> ba_params = {2, 4};
    for (auto N : sizes) {
        for (int m_attach : ba_params) run_one("barabasi_albert", N, m_attach);
    }

    // Grid graphs (use N as approximately nodes, degree param ignored)
    for (auto N : sizes) run_one("grid", N, 0);

    // Road-like graphs
    for (auto N : sizes) run_one("road", N, 0);

    out.close();
    std::cout << "Results written to scale_results.csv\n";
    return 0;
}
