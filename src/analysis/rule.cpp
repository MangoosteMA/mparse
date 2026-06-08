#include "rule.h"

#include <mparse/utils.h>

#include <algorithm>
#include <tuple>
#include <utility>

namespace mparse::analysis {

    ActionTreeNode::ActionTreeNode(const spec::Rule& rule)
        : kind_(ActionTreeNodeKind::RuleAction)
        , action_(rule.action)
        , size_(rule.items.size()) {}

    ActionTreeNode::ActionTreeNode(
        ActionTreeNodeKind kind,
        std::optional<std::string> action,
        std::optional<std::string> symbol_name,
        size_t size,
        std::vector<ActionTreeEdge> edges
    )
        : kind_(kind)
        , action_(std::move(action))
        , symbol_name_(std::move(symbol_name))
        , size_(size)
        , edges_(std::move(edges)) {}

    ActionTreeNodePtr ActionTreeNode::makeEmptySymbolReference(std::string symbol_name) {
        return ActionTreeNodePtr{
            new ActionTreeNode(
                /* kind: */ ActionTreeNodeKind::EmptySymbolReference,
                /* action: */ std::nullopt,
                /* symbol_name: */ std::move(symbol_name),
                /* size: */ 0,
                /* edges: */ {}
            )
        };
    }

    ActionTreeNodePtr ActionTreeNode::makeForwardSemanticValue() {
        return ActionTreeNodePtr{
            new ActionTreeNode(
                /* kind: */ ActionTreeNodeKind::ForwardSemanticValue,
                /* action: */ std::nullopt,
                /* symbol_name: */ std::nullopt,
                /* size: */ 1,
                /* edges: */ {}
            )
        };
    }

    ActionTreeNodePtr ActionTreeNode::cloneWithoutEdges() const {
        return ActionTreeNodePtr{
            new ActionTreeNode(
                /* kind: */ kind_,
                /* action: */ action_,
                /* symbol_name: */ symbol_name_,
                /* size: */ size_,
                /* edges: */ {}
            )
        };
    }

    ActionTreeNodePtr ActionTreeNode::withEdgeReplacingLeaf(
        size_t item_index,
        ActionTreeNodePtr node,
        std::optional<std::string> expanded_symbol_name
    ) const {
        VERIFY(node);
        VERIFY(item_index < size_);

        auto edges = edges_;
        for (const auto& edge : edges) {
            VERIFY(edge.offset_right <= item_index || edge.offset_left > item_index);
        }

        const auto offset_left = item_index;
        const auto offset_right = offset_left + node->size();
        const auto new_size = size_ - 1 + node->size();

        for (auto& edge : edges) {
            if (edge.offset_left < offset_left + 1) {
                continue;
            }

            if (node->size() == 0) {
                edge.offset_left--;
                edge.offset_right--;
            } else {
                edge.offset_left += node->size() - 1;
                edge.offset_right += node->size() - 1;
            }
        }

        edges.push_back(ActionTreeEdge{
            .node = std::move(node),
            .offset_left = offset_left,
            .offset_right = offset_right,
            .expanded_symbol_name = std::move(expanded_symbol_name),
        });

        std::ranges::stable_sort(
            edges,
            std::ranges::less{},
            [](const ActionTreeEdge& edge) {
                return std::tie(edge.offset_left, edge.offset_right);
            }
        );

        return ActionTreeNodePtr{
            new ActionTreeNode(
                /* kind: */ kind_,
                /* action: */ action_,
                /* symbol_name: */ symbol_name_,
                /* size: */ new_size,
                /* edges: */ std::move(edges)
            )
        };
    }

    namespace {

        void adjustAfterSizeChange(size_t& value, size_t old_size, size_t new_size) {
            if (new_size > old_size) {
                value += new_size - old_size;
            } else {
                value -= old_size - new_size;
            }
        }

    } // namespace

    ActionTreeNodePtr ActionTreeNode::withEdgeReplacingItem(
        size_t item_index,
        ActionTreeNodePtr node,
        std::optional<std::string> expanded_symbol_name
    ) const {
        VERIFY(node);
        VERIFY(item_index < size_);

        for (size_t edge_index = 0; edge_index < edges_.size(); edge_index++) {
            const auto& edge = edges_[edge_index];
            if (edge.offset_left > item_index) {
                break;
            }

            if (edge.offset_right <= item_index) {
                continue;
            }

            auto edges = edges_;
            auto& replaced_edge = edges[edge_index];

            const auto old_offset_right = replaced_edge.offset_right;
            const auto old_edge_size = replaced_edge.offset_right - replaced_edge.offset_left;

            auto replaced_node =
                VERIFY(replaced_edge.node)
                    ->withEdgeReplacingItem(
                        item_index - replaced_edge.offset_left,
                        std::move(node),
                        std::move(expanded_symbol_name)
                    );
            const auto new_edge_size = replaced_node->size();

            replaced_edge.node = std::move(replaced_node);
            replaced_edge.offset_right = replaced_edge.offset_left + new_edge_size;

            for (size_t current_edge_index = 0; current_edge_index < edges.size();
                 current_edge_index++) {
                if (current_edge_index == edge_index) {
                    continue;
                }

                auto& current_edge = edges[current_edge_index];
                if (current_edge.offset_left < old_offset_right) {
                    continue;
                }

                adjustAfterSizeChange(current_edge.offset_left, old_edge_size, new_edge_size);
                adjustAfterSizeChange(current_edge.offset_right, old_edge_size, new_edge_size);
            }

            size_t new_size = size_;
            adjustAfterSizeChange(new_size, old_edge_size, new_edge_size);

            return ActionTreeNodePtr{
                new ActionTreeNode(
                    /* kind: */ kind_,
                    /* action: */ action_,
                    /* symbol_name: */ symbol_name_,
                    /* size: */ new_size,
                    /* edges: */ std::move(edges)
                )
            };
        }

        return withEdgeReplacingLeaf(
            item_index,
            std::move(node),
            std::move(expanded_symbol_name)
        );
    }

    ActionTreeNodePtr ActionTreeNode::withEdgeReplacingFirstItem(
        ActionTreeNodePtr node,
        std::optional<std::string> expanded_symbol_name
    ) const {
        return withEdgeReplacingItem(
            0,
            std::move(node),
            std::move(expanded_symbol_name)
        );
    }

    ActionTreeNode& ActionTreeNode::setEdges(std::vector<ActionTreeEdge> edges) {
        edges_ = std::move(edges);
        return *this;
    }

    ActionTreeNodeKind ActionTreeNode::kind() const {
        return kind_;
    }

    const std::vector<ActionTreeEdge>& ActionTreeNode::edges() const {
        return edges_;
    }

    const std::optional<std::string>& ActionTreeNode::action() const {
        return action_;
    }

    const std::optional<std::string>& ActionTreeNode::symbolName() const {
        return symbol_name_;
    }

    size_t ActionTreeNode::size() const {
        return size_;
    }

    Rule::Rule(spec::Rule rule)
        : action_(std::make_shared<ActionTreeNode>(rule))
        , items_(std::move(rule.items)) {}

    Rule::Rule(std::vector<spec::RuleItem> items, ActionTreeNodePtr action)
        : action_(std::move(action))
        , items_(std::move(items)) {}

    Rule Rule::makeEmpty(ActionTreeNodePtr action) {
        return Rule({}, std::move(action));
    }

    bool Rule::empty() const {
        return items_.empty();
    }

    const std::vector<spec::RuleItem>& Rule::items() const {
        return items_;
    }

    ActionTreeNodePtr Rule::action() const {
        return action_;
    }

} // namespace mparse::analysis
