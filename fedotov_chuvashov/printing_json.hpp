#pragma once
#include "graph.hpp"
#include "printing.hpp"
#include <string>

namespace uni_course_cpp {
namespace json {

std::string print_vertex(const Graph::Vertex& vertex, const Graph& graph);

std::string print_edge(const Graph::Edge& edge);

std::string print_graph(const Graph& graph);
}  // namespace json
}  // namespace uni_course_cpp
