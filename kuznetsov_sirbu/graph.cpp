#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class Graph {
 public:
  using VertexId = int;
  using EdgeId = int;
  using Depth = int;
  static constexpr Depth kBaseDepth = 1;
  static constexpr Depth kDifferenceRedEdge = 2;
  static constexpr Depth kDifferenceYellowEdge = 1;
  struct Vertex {
   public:
    explicit Vertex(VertexId id) : id_(id) {}
    VertexId id() const { return id_; }

   private:
    VertexId id_ = 0;
  };

  struct Edge {
    enum class Color { Grey, Green, Yellow, Red };

   public:
    Edge(EdgeId id, VertexId from_vertex_id, VertexId to_vertex_id, Color color)
        : id_(id),
          from_vertex_id_(from_vertex_id),
          to_vertex_id_(to_vertex_id),
          color_(color) {}
    EdgeId id() const { return id_; }
    VertexId from_vertex_id() const { return from_vertex_id_; }
    VertexId to_vertex_id() const { return to_vertex_id_; }
    Color color() const { return color_; }

   private:
    Color color_ = Color::Grey;
    EdgeId id_ = 0;
    VertexId from_vertex_id_ = 0;
    VertexId to_vertex_id_ = 0;
  };

  VertexId add_vertex() {
    const VertexId new_vertex_id = get_new_vertex_id();
    vertices_.emplace_back(new_vertex_id);
    adjacency_list_[new_vertex_id] = {};

    vertex_depths_[new_vertex_id] = kBaseDepth;
    depth_to_vertices_[kBaseDepth].emplace_back(new_vertex_id);
    return new_vertex_id;
  }

  EdgeId add_edge(VertexId from_vertex_id, VertexId to_vertex_id) {
    assert(has_vertex(from_vertex_id));
    assert(has_vertex(to_vertex_id));

    const auto color = get_edge_color(from_vertex_id, to_vertex_id);
    if (color == Edge::Color::Grey) {
      set_vertex_depth(to_vertex_id, vertex_depth(from_vertex_id) + 1);
    }

    const EdgeId edge_id = get_new_edge_id();
    edges_.emplace_back(edge_id, from_vertex_id, to_vertex_id, color);
    if (from_vertex_id != edge_id) {
      adjacency_list_[from_vertex_id].emplace_back(edge_id);
    }
    adjacency_list_[to_vertex_id].emplace_back(edge_id);

    return edge_id;
  }

  bool has_vertex(VertexId vertex_id) const {
    return adjacency_list_.find(vertex_id) != adjacency_list_.end();
  }

  const std::vector<Vertex>& get_vertices() const { return vertices_; }

  const std::vector<Edge>& get_edges() const { return edges_; }

  const std::vector<EdgeId>& connected_edges_ids(VertexId vertex_id) const {
    if (!has_vertex(vertex_id)) {
      static std::vector<EdgeId> empty_edges_list;
      return empty_edges_list;
    }
    return adjacency_list_.at(vertex_id);
  }

  void set_vertex_depth(VertexId vertex_id, Depth depth) {
    depth_to_vertices_[depth].emplace_back(vertex_id);
    const auto pos = std::find(depth_to_vertices_[kBaseDepth].begin(),
                               depth_to_vertices_[kBaseDepth].end(), vertex_id);
    if (pos != depth_to_vertices_[kBaseDepth].end()) {
      depth_to_vertices_[kBaseDepth].erase(pos);
    }
    vertex_depths_[vertex_id] = depth;
  }

  bool is_connected(VertexId from_vertex_id, VertexId to_vertex_id) const {
    const auto& edges_ids = adjacency_list_.at(from_vertex_id);
    for (const auto& edge_id : edges_ids) {
      const auto& edge = edges_[edge_id];
      if (edge.from_vertex_id() == to_vertex_id ||
          edge.to_vertex_id() == to_vertex_id) {
        return true;
      }
    }
    return false;
  }

  Edge::Color get_edge_color(VertexId from_vertex_id,
                             VertexId to_vertex_id) const {
    const auto from_vertex_depth = vertex_depths_.at(from_vertex_id);
    const auto to_vertex_depth = vertex_depths_.at(to_vertex_id);

    if (from_vertex_id == to_vertex_id) {
      return Edge::Color::Green;
    }
    if (adjacency_list_.at(to_vertex_id).size() == 0) {
      return Edge::Color::Grey;
    }
    if (to_vertex_depth - from_vertex_depth == kDifferenceYellowEdge &&
        !is_connected(from_vertex_id, to_vertex_id)) {
      return Edge::Color::Yellow;
    }
    if (to_vertex_depth - from_vertex_depth == kDifferenceRedEdge) {
      return Edge::Color::Red;
    }
    throw std::runtime_error("Failed to determine color");
  }

  Depth vertex_depth(VertexId vertex_id) const {
    return vertex_depths_.at(vertex_id);
  }

  const std::vector<VertexId>& get_vertices_with_depth(Depth depth) const {
    if (depth_to_vertices_.find(depth) != depth_to_vertices_.end()) {
      return depth_to_vertices_.at(depth);
    } else {
      static std::vector<VertexId> tmp;
      return tmp;
    }
  }

  Depth depth() const { return depth_to_vertices_.size(); }

 private:
  VertexId get_new_vertex_id() { return vertex_id_counter_++; }
  EdgeId get_new_edge_id() { return edge_id_counter_++; }

  VertexId vertex_id_counter_ = 0;
  EdgeId edge_id_counter_ = 0;
  std::vector<Vertex> vertices_;
  std::vector<Edge> edges_;
  std::unordered_map<VertexId, std::vector<EdgeId> > adjacency_list_;
  std::unordered_map<VertexId, Depth> vertex_depths_;
  std::unordered_map<Depth, std::vector<VertexId> > depth_to_vertices_;
};

// graph generator

class GraphGenerator {
 public:
  static constexpr double kProbabilityRed = 0.33;
  static constexpr double kProbabilityGreen = 0.1;
  struct Params {
   public:
    Params(Graph::Depth depth, int new_vertices_count)
        : depth_(depth), new_vertices_count_(new_vertices_count) {}

    Graph::Depth depth() const { return depth_; }
    int new_vertices_count() const { return new_vertices_count_; }

   private:
    Graph::Depth depth_ = 0;
    int new_vertices_count_ = 0;
  };

  explicit GraphGenerator(Params&& params) : params_(std::move(params)) {}
  Graph generate() const {
    auto graph = Graph();
    if (params_.depth() > 0) {
      graph.add_vertex();
      generate_grey_edges(graph);
      generate_green_edges(graph);
      generate_yellow_edges(graph);
      generate_red_edges(graph);
    }
    return graph;
  }

 private:
  bool check_probability(double chance) const {
    std::mt19937 generator{std::random_device()()};
    std::bernoulli_distribution distribution(chance);
    return distribution(generator);
  }

  double probaility_generate_grey_edge(Graph::Depth current_depth,
                                       Graph::Depth graph_depth) const {
    if (graph_depth == 0) {
      return 1.0;
    } else {
      return 1.0 - static_cast<double>((current_depth - Graph::kBaseDepth)) /
                       (graph_depth - Graph::kBaseDepth);
    }
  }

  Graph::VertexId get_random_vertex_id(int size) const {
    std::random_device rand_device;
    std::mt19937 gen(rand_device());
    std::uniform_int_distribution<> distrib(0, size - 1);
    return distrib(gen);
  }

  std::vector<Graph::VertexId> get_unconnected_vertex_ids(
      const Graph& graph,
      Graph::VertexId vertex_from_id,
      const std::vector<Graph::VertexId>& vertex_ids) const {
    std::vector<Graph::VertexId> not_connected_vertex_ids;
    for (const auto& vertex_to_id : vertex_ids) {
      if (!graph.is_connected(vertex_from_id, vertex_to_id)) {
        not_connected_vertex_ids.emplace_back(vertex_to_id);
      }
    }
    return not_connected_vertex_ids;
  }

  void try_generate_grey_edge(Graph& graph,
                              Graph::Depth current_depth,
                              Graph::VertexId vertex_id) const {
    const auto probability =
        probaility_generate_grey_edge(current_depth, params_.depth());
    if (check_probability(probability)) {
      const Graph::VertexId next_vertex_id = graph.add_vertex();
      graph.add_edge(vertex_id, next_vertex_id);
    }
  }

  void generate_grey_edges(Graph& graph) const {
    for (Graph::Depth current_depth = Graph::kBaseDepth;
         current_depth <= params_.depth(); current_depth++) {
      const auto vertices_with_last_depth =
          graph.get_vertices_with_depth(current_depth);
      if (graph.depth() != current_depth) {
        break;
      }
      for (const auto& vertex_id : vertices_with_last_depth) {
        for (int i = 0; i < params_.new_vertices_count(); ++i) {
          try_generate_grey_edge(graph, current_depth, vertex_id);
        }
      }
    }
  }

  void try_generate_yellow_edge(Graph& graph,
                                Graph::VertexId vertex_from_id,
                                Graph::VertexId vertex_to_id) const {
    const Graph::Depth vertex_from_depth = graph.vertex_depth(vertex_from_id);
    graph.add_edge(vertex_from_id, vertex_to_id);
  }

  void generate_yellow_edges(Graph& graph) const {
    for (const auto& vertex_from : graph.get_vertices()) {
      const auto vertex_from_id = vertex_from.id();
      const Graph::Depth vertex_depth = graph.vertex_depth(vertex_from_id);
      const double probability_generate =
          1.0 - static_cast<double>((vertex_depth - Graph::kBaseDepth)) /
                    (graph.depth() - Graph::kBaseDepth);
      if (!check_probability(probability_generate)) {
        const auto& vertex_ids = graph.get_vertices_with_depth(
            vertex_depth + Graph::kDifferenceYellowEdge);
        const auto not_connected_vertex_ids =
            get_unconnected_vertex_ids(graph, vertex_from_id, vertex_ids);
        if (!not_connected_vertex_ids.empty()) {
          const Graph::VertexId vertex_to_id = not_connected_vertex_ids.at(
              get_random_vertex_id(not_connected_vertex_ids.size()));
          try_generate_yellow_edge(graph, vertex_from_id, vertex_to_id);
        }
      }
    }
  }

  void try_generate_red_edge(Graph& graph,
                             Graph::VertexId vertex_from_id,
                             Graph::VertexId vertex_to_id) const {
    const Graph::Depth vertex_from_depth = graph.vertex_depth(vertex_from_id);
    graph.add_edge(vertex_from_id, vertex_to_id);
  }

  void generate_red_edges(Graph& graph) const {
    for (const auto& vertex_from : graph.get_vertices()) {
      if (!check_probability(kProbabilityRed)) {
        continue;
      }
      const auto vertex_from_id = vertex_from.id();
      const Graph::Depth vertex_depth = graph.vertex_depth(vertex_from_id);
      const auto& vertex_ids = graph.get_vertices_with_depth(
          vertex_depth + Graph::kDifferenceRedEdge);
      if (!vertex_ids.empty()) {
        const Graph::VertexId vertex_to_id =
            vertex_ids.at(get_random_vertex_id(vertex_ids.size()));
        try_generate_red_edge(graph, vertex_from_id, vertex_to_id);
      }
    }
  }

  void try_generate_green_edge(Graph& graph, Graph::VertexId vertex_id) const {
    if (check_probability(kProbabilityGreen)) {
      graph.add_edge(vertex_id, vertex_id);
    }
  }

  void generate_green_edges(Graph& graph) const {
    for (const auto& vertex : graph.get_vertices()) {
      try_generate_green_edge(graph, vertex.id());
    }
  }

  Params params_ = Params(0, 0);
};

namespace printing {
namespace json {

std::string print_vertex(const Graph::Vertex& vertex, const Graph& graph) {
  std::string result_json_vertex = "{\"id\":";
  result_json_vertex += std::to_string(vertex.id()) + ",";
  const std::vector<Graph::EdgeId>& edge_ids =
      graph.connected_edges_ids(vertex.id());

  result_json_vertex += "\"edge_ids\":[";
  for (const auto& id : edge_ids) {
    result_json_vertex += std::to_string(id) + ",";
  }

  if (edge_ids.size())
    result_json_vertex.pop_back();
  result_json_vertex += "],";
  result_json_vertex +=
      "\"depth\":" + std::to_string(graph.vertex_depth(vertex.id())) + "}";
  return result_json_vertex;
}

std::string print_edge_color(const Graph::Edge& edge) {
  switch (edge.color()) {
    case Graph::Edge::Color::Grey:
      return "grey";
    case Graph::Edge::Color::Red:
      return "red";
    case Graph::Edge::Color::Yellow:
      return "yellow";
    case Graph::Edge::Color::Green:
      return "green";
  }
  throw std::runtime_error("Failed to determine color");
}

std::string print_edge(const Graph::Edge& edge) {
  std::string result_json_edge =
      "{\"id\":" + std::to_string(edge.id()) + ",\"vertex_ids\":" + "[" +
      std::to_string(edge.from_vertex_id()) + "," +
      std::to_string(edge.to_vertex_id()) + "]," + "\"color\":";
  std::string edge_color = print_edge_color(edge);
  result_json_edge += "\"" + edge_color + "\"" + "}";
  return result_json_edge;
}

std::string print_graph(const Graph& graph) {
  std::string result = "";
  result += "{\"depth\":" + std::to_string(graph.depth()) + ",";
  result += "\"vertices\":[";

  for (const auto& vertex : graph.get_vertices()) {
    result += print_vertex(vertex, graph) + ",";
  }
  if (result.back() == ',')
    result.pop_back();
  result += "],\"edges\":[";

  for (const auto& edge : graph.get_edges()) {
    result += print_edge(edge) + ",";
  }
  if (result.back() == ',')
    result.pop_back();
  result += "]}\n";
  return result;
}
}  // namespace json
}  // namespace printing

void write_to_file(const std::string& graph_json,
                   const std::string& file_name) {
  std::ofstream file(file_name);
  file << graph_json;
}

constexpr int kInputSize = 256;

Graph::Depth handle_depth_input() {
  std::cout << "Depth: ";
  int depth = 0;
  while (!(std::cin >> depth) || depth < 0) {
    std::cout << "Invalid value" << std::endl << "Depth: ";
    std::cin.clear();
    std::cin.ignore(kInputSize, '\n');
  }
  return depth;
}

int handle_new_vertices_count_input() {
  std::cout << "Vertices count: ";
  int new_vertices_count = 0;
  while (!(std::cin >> new_vertices_count) || new_vertices_count < 0) {
    std::cout << "Invalid value" << std::endl << "Vertices count: ";
    std::cin.clear();
    std::cin.ignore(kInputSize, '\n');
  }
  return new_vertices_count;
}

int main() {
  const int depth = handle_depth_input();
  const int new_vertices_count = handle_new_vertices_count_input();

  auto params = GraphGenerator::Params(depth, new_vertices_count);
  const auto generator = GraphGenerator(std::move(params));
  const auto graph = generator.generate();

  const auto graph_json = printing::json::print_graph(graph);
  std::cout << graph_json << '\n';
  write_to_file(graph_json, "graph.json");

  return 0;
}
