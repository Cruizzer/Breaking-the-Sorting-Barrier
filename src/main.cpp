#include "graph.hpp"
#include "graph_generator.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/bmssp.hpp"
#include "benchmark/benchmark.hpp"

#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " --generate TYPE SIZE [OPTIONS]\n";
    std::cout << "\nGraph Generation:\n";
    std::cout << "  --generate random N [degree]    Generate Erdos-Renyi G(n,m) sparse graph (alias)\n";
    std::cout << "  --generate er N [degree]        Generate Erdos-Renyi G(n,m) sparse graph\n";
    std::cout << "                                  N = number of vertices, degree = avg degree (default: 4)\n";
    std::cout << "  --generate ba N [m]             Generate Barabasi-Albert graph\n";
    std::cout << "                                  N = number of vertices, m = edges/new vertex (default: 2)\n";
    std::cout << "  --generate grid R C             Generate RxC grid graph\n";
    std::cout << "  --generate road N               Generate road-like network\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --trials K                      Run K trials with random sources (default: 1)\n";
    std::cout << "  --report FILE                   Save benchmark report to CSV file\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --generate er 10000 4\n";
    std::cout << "  " << program_name << " --generate ba 10000 3\n";
    std::cout << "  " << program_name << " --generate random 100000 6 --trials 5\n";
    std::cout << "  " << program_name << " --generate grid 100 100 --report results.csv\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (std::string(argv[1]) != "--generate") {
        std::cerr << "Error: First argument must be --generate\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse generation parameters
    std::string type = argv[2];
    Graph graph;
    
    std::cout << "=== Graph Generation ===\n";
    
    if (type == "random" || type == "er") {
        if (argc < 4) {
            std::cerr << "Error: random requires size parameter\n";
            return 1;
        }
        size_t n = std::stoull(argv[3]);
        double avg_degree = (argc >= 5 && std::string(argv[4]).find("--") != 0) 
                            ? std::stod(argv[4]) : 4.0;
        
        std::cout << "Generating Erdos-Renyi G(n,m) graph...\n";
        std::cout << "  Vertices: " << n << "\n";
        std::cout << "  Avg degree: " << avg_degree << "\n";
        graph = generate_erdos_renyi_graph(n, avg_degree);

    } else if (type == "ba") {
        if (argc < 4) {
            std::cerr << "Error: ba requires size parameter\n";
            return 1;
        }
        size_t n = std::stoull(argv[3]);
        size_t m_attach = (argc >= 5 && std::string(argv[4]).find("--") != 0)
                          ? std::stoull(argv[4]) : 2;

        std::cout << "Generating Barabasi-Albert graph...\n";
        std::cout << "  Vertices: " << n << "\n";
        std::cout << "  Edges per new vertex (m): " << m_attach << "\n";
        graph = generate_barabasi_albert_graph(n, m_attach);
        
    } else if (type == "grid") {
        if (argc < 5) {
            std::cerr << "Error: grid requires rows and cols\n";
            return 1;
        }
        size_t rows = std::stoull(argv[3]);
        size_t cols = std::stoull(argv[4]);
        
        std::cout << "Generating grid graph...\n";
        std::cout << "  Dimensions: " << rows << "x" << cols << "\n";
        graph = generate_grid_graph(rows, cols);
        
    } else if (type == "road") {
        if (argc < 4) {
            std::cerr << "Error: road requires size parameter\n";
            return 1;
        }
        size_t n = std::stoull(argv[3]);
        
        std::cout << "Generating road-like network...\n";
        std::cout << "  Vertices: " << n << "\n";
        graph = generate_road_network(n);
        
    } else {
        std::cerr << "Error: Unknown graph type '" << type << "'\n";
        return 1;
    }
    
    std::cout << "✓ Graph generated: " << graph.size() << " vertices\n";
    
    // Parse options
    size_t num_trials = 1;
    std::string report_file;
    
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trials" && i + 1 < argc) {
            num_trials = std::stoull(argv[++i]);
        } else if (arg == "--report" && i + 1 < argc) {
            report_file = argv[++i];
        }
    }
    
    // Run benchmarks
    std::cout << "\n=== Running Benchmarks ===\n";
    std::cout << "Trials: " << num_trials << "\n";
    
    if (num_trials == 1) {
        // Single trial from vertex 0
        auto result = benchmark::compare_algorithms(
            algorithms::dijkstra,
            algorithms::bmssp,
            graph,
            0
        );
        
        benchmark::print_comparison(result);

        // If algorithms disagree, print per-vertex distances and reconstructed paths
        if (!result.results_match) {
            std::cout << "\n--- Detailed diagnostics (mismatched vertices) ---\n";
            const auto& dists_dij = result.dijkstra_result.distances;
            const auto& dists_bm = result.bmssp_result.distances;
            for (size_t v = 0; v < dists_dij.size(); ++v) {
                double a = dists_dij[v];
                double b = dists_bm[v];
                if (std::isinf(a) && std::isinf(b)) continue;
                double diff = std::abs(a - b);
                if (diff > 1e-6) {
                    std::cout << "Vertex " << v << ": Dijkstra=" << a << " BMSSP=" << b << " diff=" << diff << "\n";
                    auto path_dij = algorithms::dijkstra_path(graph, 0, v);
                    auto path_bm = algorithms::bmssp_path(graph, 0, v);
                    std::cout << "  Dijkstra path: ";
                    for (auto x : path_dij) std::cout << x << " ";
                    std::cout << "\n  BMSSP path:    ";
                    for (auto x : path_bm) std::cout << x << " ";
                    std::cout << "\n";
                        // Print adjacency lists for nodes involved in the differing paths
                        std::vector<Vertex> inspect = {0, v};
                        inspect.insert(inspect.end(), path_dij.begin(), path_dij.end());
                        inspect.insert(inspect.end(), path_bm.begin(), path_bm.end());
                        // unique
                        std::sort(inspect.begin(), inspect.end());
                        inspect.erase(std::unique(inspect.begin(), inspect.end()), inspect.end());
                        for (auto u : inspect) {
                            std::cout << "  Neighbors of " << u << ": ";
                            for (const auto& e : graph[u]) {
                                std::cout << e.to << "(" << e.weight << ") ";
                            }
                            std::cout << "\n";
                        }
                }
            }
            std::cout << "--- end diagnostics ---\n";
        }
        
        if (!report_file.empty()) {
            std::vector<benchmark::ComparisonResult> results;
            results.push_back(result);
            benchmark::generate_report(results, report_file);
        }
        
    } else {
        // Multiple trials
        std::cout << "\nRunning " << num_trials << " trials from random source vertices...\n";
        
        std::vector<benchmark::ComparisonResult> all_results;
        
        for (size_t trial = 0; trial < num_trials; ++trial) {
            Vertex source = (trial * 1234567) % graph.size();  // Pseudo-random source
            
            std::cout << "\nTrial " << (trial + 1) << "/" << num_trials 
                      << " (source: " << source << ")...\n";
            
            auto result = benchmark::compare_algorithms(
                algorithms::dijkstra,
                algorithms::bmssp,
                graph,
                source
            );
            
            all_results.push_back(result);
            
            std::cout << "  Dijkstra: " << result.dijkstra_result.execution_time_ms << " ms\n";
            std::cout << "  BMSSP:    " << result.bmssp_result.execution_time_ms << " ms\n";
            std::cout << "  Speedup:  " << result.speedup_factor << "x\n";
        }
        
        // Calculate average speedup
        double avg_speedup = 0.0;
        double avg_dijkstra_time = 0.0;
        double avg_bmssp_time = 0.0;
        
        for (const auto& r : all_results) {
            avg_speedup += r.speedup_factor;
            avg_dijkstra_time += r.dijkstra_result.execution_time_ms;
            avg_bmssp_time += r.bmssp_result.execution_time_ms;
        }
        
        avg_speedup /= num_trials;
        avg_dijkstra_time /= num_trials;
        avg_bmssp_time /= num_trials;
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "AVERAGE RESULTS (" << num_trials << " trials)\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "Dijkstra: " << avg_dijkstra_time << " ms\n";
        std::cout << "BMSSP:    " << avg_bmssp_time << " ms\n";
        std::cout << "Average Speedup: " << avg_speedup << "x\n";
        std::cout << std::string(70, '=') << "\n";
        
        if (!report_file.empty()) {
            benchmark::generate_report(all_results, report_file);
        }
    }
    
    std::cout << "\nDone.\n";
    return 0;
}
