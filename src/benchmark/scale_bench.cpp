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
    out << "graph_type,size,degree,trials,dijkstra_ms,bmssp_ms,speedup,reachable,avg_distance,max_distance\n";

    auto run_one = [&](const std::string& type, size_t N, int deg){
        double total_d_ms = 0.0;
        double total_b_ms = 0.0;
        double total_speedup = 0.0;
        size_t total_reachable = 0;
        double total_avgdist = 0.0;
        double total_maxdist = 0.0;

        for (size_t t = 0; t < trials; ++t) {
            Graph g;
            if (type == "random") {
                g = generate_random_graph(N, deg);
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

            total_d_ms += comp.dijkstra_result.execution_time_ms;
            total_b_ms += comp.bmssp_result.execution_time_ms;
            total_speedup += comp.speedup_factor;
            total_reachable += comp.dijkstra_result.reachable_vertices;
            total_avgdist += comp.dijkstra_result.avg_distance;
            total_maxdist += comp.dijkstra_result.max_distance;
        }

        double avg_d = total_d_ms / trials;
        double avg_b = total_b_ms / trials;
        double avg_speed = total_speedup / trials;
        double avg_reachable = static_cast<double>(total_reachable) / trials;
        double avg_dist = total_avgdist / trials;
        double avg_max = total_maxdist / trials;

        out << type << "," << N << "," << deg << "," << trials << ","
            << avg_d << "," << avg_b << "," << avg_speed << ","
            << avg_reachable << "," << avg_dist << "," << avg_max << "\n";

        std::cout << "Completed: " << type << " N=" << N << " deg=" << deg << " -> d=" << avg_d << " ms, b=" << avg_b << " ms\n";
    };

    // Random graphs
    for (auto N : sizes) {
        for (int deg : degrees) run_one("random", N, deg);
    }

    // Grid graphs (use N as approximately nodes, degree param ignored)
    for (auto N : sizes) run_one("grid", N, 0);

    // Road-like graphs
    for (auto N : sizes) run_one("road", N, 0);

    out.close();
    std::cout << "Results written to scale_results.csv\n";
    return 0;
}
