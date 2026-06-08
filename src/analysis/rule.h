#pragma once

#include <spec/data.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    class ActionTreeNode;

    using ActionTreeNodePtr = std::shared_ptr<ActionTreeNode>;

    enum class ActionTreeNodeKind {
        RuleAction,
        EmptySymbolReference,
        ForwardSemanticValue,
    };

    struct ActionTreeEdge {
        ActionTreeNodePtr node;
        size_t offset_left;
        size_t offset_right;
        std::optional<std::string> expanded_symbol_name;
    };

    class ActionTreeNode {
    public:
        ActionTreeNode(const spec::Rule& rule);

        static ActionTreeNodePtr makeEmptySymbolReference(std::string symbol_name);
        static ActionTreeNodePtr makeForwardSemanticValue();

        ActionTreeNodePtr cloneWithoutEdges() const;

        ActionTreeNodePtr withEdgeReplacingItem(
            size_t item_index,
            ActionTreeNodePtr node,
            std::optional<std::string> expanded_symbol_name = std::nullopt
        ) const;

        ActionTreeNodePtr withEdgeReplacingFirstItem(
            ActionTreeNodePtr node,
            std::optional<std::string> expanded_symbol_name = std::nullopt
        ) const;

        ActionTreeNode& setEdges(std::vector<ActionTreeEdge> edges);

        ActionTreeNodeKind kind() const;

        const std::optional<std::string>& action() const;
        const std::optional<std::string>& symbolName() const;
        const std::vector<ActionTreeEdge>& edges() const;

        size_t size() const;

    private:
        ActionTreeNodePtr withEdgeReplacingLeaf(
            size_t item_index,
            ActionTreeNodePtr node,
            std::optional<std::string> expanded_symbol_name
        ) const;

        ActionTreeNode(
            ActionTreeNodeKind kind,
            std::optional<std::string> action,
            std::optional<std::string> symbol_name,
            size_t size,
            std::vector<ActionTreeEdge> edges
        );

        ActionTreeNodeKind kind_;
        std::optional<std::string> action_;
        std::optional<std::string> symbol_name_;
        size_t size_;
        std::vector<ActionTreeEdge> edges_;
    };

    class Rule {
    public:
        Rule(spec::Rule rule);
        Rule(std::vector<spec::RuleItem> items, ActionTreeNodePtr action);

        static Rule makeEmpty(ActionTreeNodePtr action);

        bool empty() const;
        const std::vector<spec::RuleItem>& items() const;
        ActionTreeNodePtr action() const;

    private:
        ActionTreeNodePtr action_;
        std::vector<spec::RuleItem> items_;
    };

} // namespace mparse::analysis
