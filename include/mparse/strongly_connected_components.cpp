#include "strongly_connected_components.h"

#include <algorithm>

namespace mparse {

    StronglyConnectedComponents::StronglyConnectedComponents(size_t size)
        : graph_(size) {}

    void StronglyConnectedComponents::addEdge(size_t from, size_t to) {
        graph_[from].push_back(to);
    }

    std::vector<size_t> StronglyConnectedComponents::solve() {
        current_id_ = 0;
        current_scc_ = 0;
        ids_.assign(graph_.size(), kUnvisited);
        low_links_.assign(graph_.size(), kUnvisited);
        sccs_.assign(graph_.size(), kUnvisited);
        stack_.clear();

        for (size_t vertex = 0; vertex < graph_.size(); vertex++) {
            if (ids_[vertex] == kUnvisited) {
                dfs(vertex);
            }
        }

        for (auto& scc : sccs_) {
            scc = current_scc_ - 1 - scc;
        }

        return sccs_;
    }

    void StronglyConnectedComponents::dfs(size_t vertex) {
        stack_.push_back(vertex);
        ids_[vertex] = low_links_[vertex] = current_id_++;

        for (const auto next_vertex : graph_[vertex]) {
            if (ids_[next_vertex] == kUnvisited) {
                dfs(next_vertex);
                low_links_[vertex] = std::min(
                    low_links_[vertex],
                    low_links_[next_vertex]
                );
            } else if (sccs_[next_vertex] == kUnvisited) {
                low_links_[vertex] = std::min(low_links_[vertex], ids_[next_vertex]);
            }
        }

        if (ids_[vertex] != low_links_[vertex]) {
            return;
        }

        size_t next_vertex;
        do {
            next_vertex = stack_.back();
            stack_.pop_back();
            sccs_[next_vertex] = current_scc_;
        } while (next_vertex != vertex);

        current_scc_++;
    }

} // namespace mparse
