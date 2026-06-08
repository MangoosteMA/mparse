#pragma once

#include <cstddef>
#include <limits>
#include <vector>

namespace mparse {

    class StronglyConnectedComponents {
    public:
        explicit StronglyConnectedComponents(size_t size);

        void addEdge(size_t from, size_t to);

        std::vector<size_t> solve();

    private:
        static constexpr size_t kUnvisited = std::numeric_limits<std::size_t>::max();

        void dfs(size_t vertex);

        std::vector<std::vector<size_t>> graph_;

        size_t current_id_ = 0;
        size_t current_scc_ = 0;
        std::vector<size_t> ids_;
        std::vector<size_t> low_links_;
        std::vector<size_t> sccs_;
        std::vector<size_t> stack_;
    };

} // namespace mparse
