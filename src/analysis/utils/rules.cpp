#include "rules.h"

#include <mparse/utils.h>
#include <spec/data.h>

#include <iterator>
#include <optional>
#include <utility>
#include <variant>

namespace mparse::analysis {

    spec::RuleItem makeSymbolItem(const SymbolPtr& symbol) {
        VERIFY(symbol);

        spec::RuleItemSymbol item_symbol{
            .name = symbol->name(),
        };
        item_symbol.arguments.reserve(symbol->arguments().size());

        for (const auto& argument : symbol->arguments()) {
            item_symbol.arguments.push_back(argument.name);
        }

        return spec::RuleItem{
            .value = std::move(item_symbol),
        };
    }

    Rule makeForwardSemanticValueRule(const SymbolPtr& symbol) {
        return Rule(
            {makeSymbolItem(symbol)},
            ActionTreeNode::makeForwardSemanticValue()
        );
    }

    Rule makeRuleWithItems(const Rule& rule, std::vector<spec::RuleItem> items) {
        return Rule(std::move(items), VERIFY(rule.action()));
    }

    Rule replaceSymbol(
        const Rule& rule,
        size_t item_index,
        const SymbolPtr& replacement_symbol
    ) {
        VERIFY(replacement_symbol);
        VERIFY(item_index < rule.items().size());

        auto items = rule.items();
        auto* item_symbol =
            VERIFY(std::get_if<spec::RuleItemSymbol>(&items[item_index].value));
        item_symbol->name = replacement_symbol->name();
        return makeRuleWithItems(rule, std::move(items));
    }

    Rule replaceFirstSymbol(const Rule& rule, const SymbolPtr& replacement_symbol) {
        return replaceSymbol(rule, 0, replacement_symbol);
    }

    Rule replaceFirstSymbolWithRule(const Rule& rule, const Rule& replacement_rule) {
        VERIFY(!rule.items().empty());
        const auto* first_symbol =
            VERIFY(std::get_if<spec::RuleItemSymbol>(&rule.items().front().value));

        auto items = replacement_rule.items();
        items.insert(
            items.end(),
            std::next(rule.items().begin()),
            rule.items().end()
        );

        return Rule(
            std::move(items),
            VERIFY(rule.action())
                ->withEdgeReplacingFirstItem(
                    VERIFY(replacement_rule.action()),
                    first_symbol->name
                )
        );
    }

    Rule collapseItem(
        const Rule& rule,
        size_t item_index,
        ActionTreeNodePtr item_action,
        std::optional<std::string> expanded_symbol_name
    ) {
        VERIFY(item_action);
        VERIFY(item_index < rule.items().size());

        auto items = rule.items();
        items.erase(std::next(items.begin(), item_index));
        return Rule(
            std::move(items),
            VERIFY(rule.action())
                ->withEdgeReplacingItem(
                    item_index,
                    std::move(item_action),
                    std::move(expanded_symbol_name)
                )
        );
    }

    Rule collapseFirstItem(
        const Rule& rule,
        ActionTreeNodePtr item_action,
        std::optional<std::string> expanded_symbol_name
    ) {
        return collapseItem(
            rule,
            0,
            std::move(item_action),
            std::move(expanded_symbol_name)
        );
    }

    std::optional<std::string> firstSymbolItemName(const Rule& rule) {
        if (rule.items().empty()) {
            return std::nullopt;
        }

        const auto* first_symbol = std::get_if<spec::RuleItemSymbol>(&rule.items().front().value);
        if (!first_symbol) {
            return std::nullopt;
        }

        return first_symbol->name;
    }

    std::optional<SymbolPtr> firstSymbolItem(
        const SymbolsStorage& symbols_storage,
        const Rule& rule
    ) {
        const auto symbol_name = firstSymbolItemName(rule);
        if (!symbol_name) {
            return std::nullopt;
        }

        return symbols_storage.getSymbolByName(*symbol_name);
    }

    bool isExactSymbolReference(
        const spec::RuleItemSymbol& item_symbol,
        const SymbolPtr& symbol
    ) {
        VERIFY(symbol);
        if (item_symbol.name != symbol->name()) {
            return false;
        }

        const auto& symbol_arguments = symbol->arguments();
        if (item_symbol.arguments.size() != symbol_arguments.size()) {
            return false;
        }

        for (size_t index = 0; index < symbol_arguments.size(); index++) {
            if (item_symbol.arguments[index] != symbol_arguments[index].name) {
                return false;
            }
        }

        return true;
    }

    bool startsWithSymbol(const Rule& rule, const std::string& symbol_name) {
        const auto first_symbol_name = firstSymbolItemName(rule);
        if (!first_symbol_name) {
            return false;
        }

        return *first_symbol_name == symbol_name;
    }

    bool startsWithSymbol(const Rule& rule, const SymbolPtr& symbol) {
        VERIFY(symbol);
        return startsWithSymbol(rule, symbol->name());
    }

    bool startsWithExactSymbol(const Rule& rule, const SymbolPtr& symbol) {
        if (rule.items().empty()) {
            return false;
        }

        const auto* item_symbol =
            std::get_if<spec::RuleItemSymbol>(&rule.items().front().value);
        return item_symbol && isExactSymbolReference(*item_symbol, symbol);
    }

} // namespace mparse::analysis
