#pragma once
#include "graph.hpp"
#include "bmssp_algo.hpp"
#include <vector>

namespace algorithms {

std::vector<Weight> bmssp(const Graph& graph, Vertex source);
Weight bmssp_single_target(const Graph& graph, Vertex source, Vertex target);
std::vector<Vertex> bmssp_path(const Graph& graph, Vertex source, Vertex target);

} // namespace algorithms