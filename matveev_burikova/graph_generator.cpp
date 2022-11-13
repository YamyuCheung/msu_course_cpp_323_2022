#include "graph_generator.hpp"
#include "graph.hpp"

#include <atomic>
#include <cassert>
#include <execution>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <thread>
#include <vector>

namespace {
using Graph = uni_course_cpp::Graph;
using GraphGenerator = uni_course_cpp::GraphGenerator;

const int kMaxThreadsCount = std::thread::hardware_concurrency();
constexpr float kGreenEdgeGenerationProbability = 0.1;
constexpr float kRedEdgeGenerationProbability = 1.0 / 3;

using JobCallback = std::function<void()>;

const std::vector<Graph::VertexId> get_unconnected_vertex_ids(
    Graph graph,
    Graph::VertexId vertex_id,
    const std::vector<Graph::VertexId>& vertex_ids_on_depth) {
  auto unconnected_vertex_ids = vertex_ids_on_depth;
  for (auto vertex_id_iterator = unconnected_vertex_ids.begin();
       vertex_id_iterator != unconnected_vertex_ids.end();) {
    if (graph.has_edge(vertex_id, *vertex_id_iterator))
      unconnected_vertex_ids.erase(vertex_id_iterator);
    else
      ++vertex_id_iterator;
  }
  return unconnected_vertex_ids;
}
bool check_probability(double probability) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::bernoulli_distribution d(probability);
  return d(gen);
}

Graph::VertexId get_random_vertex_id(
    const std::vector<Graph::VertexId>& vertex_ids_list) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, vertex_ids_list.size() - 1);
  return vertex_ids_list[distrib(gen)];
}

void generate_green_edges(Graph& graph, std::mutex& graph_mutex) {
  const auto try_generate_green_edge =
      [&graph, &graph_mutex](
          const std::pair<Graph::VertexId, Graph::Vertex>& vertex_pair) {
        if (check_probability(kGreenEdgeGenerationProbability)) {
          const std::lock_guard<std::mutex> guard(graph_mutex);
          graph.add_edge(vertex_pair.second.id(), vertex_pair.second.id());
        }
      };
  std::for_each(graph.get_vertices().begin(), graph.get_vertices().end(),
                try_generate_green_edge);
}

void generate_yellow_edges(Graph& graph, std::mutex& graph_mutex) {
  const Graph::Depth depth = graph.depth();
  const float step = 1.0 / (depth - 2);
  for (Graph::Depth current_depth = uni_course_cpp::kInitialDepth + 1;
       current_depth < depth; ++current_depth) {
    const auto& vertex_ids_on_current_depth =
        graph.get_vertex_ids_on_depth(current_depth);

    const auto try_generate_yellow_edge =
        [&graph, &graph_mutex, step,
         current_depth](const Graph::VertexId from_vertex_id) {
          if (check_probability(step * (current_depth - 1))) {
            const std::vector<Graph::VertexId> not_connected_vertex_ids =
                get_unconnected_vertex_ids(
                    graph, from_vertex_id,
                    graph.get_vertex_ids_on_depth(current_depth + 1));
            if (not_connected_vertex_ids.size() == 0)
              return;
            const auto to_vertex_id =
                get_random_vertex_id(not_connected_vertex_ids);
            const std::lock_guard<std::mutex> guard(graph_mutex);
            graph.add_edge(from_vertex_id, to_vertex_id);
          }
        };
    std::for_each(vertex_ids_on_current_depth.begin(),
                  vertex_ids_on_current_depth.end(), try_generate_yellow_edge);
  }
}

void generate_red_edges(Graph& graph, std::mutex& graph_mutex) {
  const Graph::Depth depth = graph.depth();

  for (Graph::Depth current_depth = uni_course_cpp::kInitialDepth;
       current_depth < depth - 1; ++current_depth) {
    const auto& vertex_ids_on_current_depth =
        graph.get_vertex_ids_on_depth(current_depth);
    const auto& vertex_ids_on_following_depth =
        graph.get_vertex_ids_on_depth(current_depth + 2);

    const auto try_generate_red_edge =
        [&graph, &graph_mutex,
         &vertex_ids_on_following_depth](const Graph::VertexId from_vertex_id) {
          const auto to_vertex_id =
              get_random_vertex_id(vertex_ids_on_following_depth);
          if (check_probability(kRedEdgeGenerationProbability)) {
            const std::lock_guard<std::mutex> guard(graph_mutex);
            graph.add_edge(from_vertex_id, to_vertex_id);
          }
        };
    std::for_each(vertex_ids_on_current_depth.begin(),
                  vertex_ids_on_current_depth.end(), try_generate_red_edge);
  }
}

JobCallback get_job(std::queue<JobCallback>& jobs) {
  assert(!jobs.empty());
  auto job = jobs.front();
  jobs.pop();
  return job;
}

Graph::VertexId safe_add_connected_vertex(Graph& graph,
                                          Graph::VertexId from_vertex_id,
                                          std::mutex& graph_mutex) {
  const std::lock_guard<std::mutex> guard(graph_mutex);
  const auto to_vertex_id = graph.add_vertex();
  graph.add_edge(from_vertex_id, to_vertex_id);
  return to_vertex_id;
}
}  // namespace

namespace uni_course_cpp {
void GraphGenerator::generate_grey_branch(Graph& graph,
                                          std::mutex& graph_mutex,
                                          Graph::VertexId from_vertex_id,
                                          Graph::Depth current_depth) const {
  const Graph::Depth depth = params_.depth();
  if (current_depth > params_.depth())
    return;
  float step = 1.0 / (depth - 1);
  for (int vertices_count = 0; vertices_count < params_.new_vertices_count();
       ++vertices_count) {
    if (check_probability(1 - (current_depth - 2) * step)) {
      const auto new_vertex_id =
          safe_add_connected_vertex(graph, from_vertex_id, graph_mutex);
      generate_grey_branch(graph, graph_mutex, new_vertex_id,
                           current_depth + 1);
    }
  }
}

void GraphGenerator::generate_grey_edges(Graph& graph,
                                         Graph::VertexId base_vertex_id) const {
  std::mutex graph_mutex;
  std::atomic<bool> should_terminate = false;
  auto jobs = std::queue<JobCallback>();

  const Graph::Depth params_depth = params_.depth();
  const int new_vertices_count = params_.new_vertices_count();

  std::atomic<int> jobs_atomic = new_vertices_count;

  for (int i = 0; i < new_vertices_count; ++i) {
    int current_depth = kInitialDepth;
    const auto new_vertex_id = graph.add_vertex();
    graph.add_edge(base_vertex_id, new_vertex_id);
    current_depth++;
    const auto job = [this, current_depth, new_vertex_id, &graph,
                      &graph_mutex]() {
      generate_grey_branch(graph, graph_mutex, new_vertex_id,
                           current_depth + 1);
    };
    jobs.push(job);
  }

  const auto worker = [&should_terminate, &jobs_atomic, &jobs]() {
    while (true) {
      if (should_terminate) {
        return;
      }
      const auto job_optional = [&jobs_atomic, &should_terminate,
                                 &jobs]() -> std::optional<JobCallback> {
        if (!jobs.empty()) {
          jobs_atomic = jobs_atomic.load() - 1;
          return get_job(jobs);
        }
        return std::nullopt;
      }();
      if (job_optional.has_value()) {
        const auto& job = job_optional.value();
        job();
      }
    }
  };

  const auto threads_count = std::min(kMaxThreadsCount, new_vertices_count);
  auto threads = std::vector<std::thread>();
  threads.reserve(threads_count);
  for (int i = 0; i < threads_count; ++i)
    threads.push_back(std::thread(worker));

  while (jobs_atomic.load() != 0) {
  }
  should_terminate = true;
  for (auto& thread : threads) {
    thread.join();
  }
}

Graph GraphGenerator::generate() const {
  std::mutex graph_mutex;
  auto graph = Graph();
  if (params_.depth() == 0)
    return graph;
  const Graph::VertexId base_vertex_id = graph.add_vertex();
  if (params_.new_vertices_count() == 0)
    return graph;
  generate_grey_edges(graph, base_vertex_id);

  std::thread green_thread(generate_green_edges, std::ref(graph),
                           std::ref(graph_mutex));
  std::thread yellow_thread(&generate_yellow_edges, std::ref(graph),
                            std::ref(graph_mutex));
  std::thread red_thread(&generate_red_edges, std::ref(graph),
                         std::ref(graph_mutex));

  green_thread.join();
  yellow_thread.join();
  red_thread.join();

  return graph;
}
}  // namespace uni_course_cpp
