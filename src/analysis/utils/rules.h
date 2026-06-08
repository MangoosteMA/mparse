#pragma once

#include <analysis/rule.h>
#include <analysis/symbol.h>
#include <analysis/symbols_storage.h>

#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    spec::RuleItem makeSymbolItem(const SymbolPtr& symbol);
    Rule makeForwardSemanticValueRule(const SymbolPtr& symbol);
    Rule makeRuleWithItems(const Rule& rule, std::vector<spec::RuleItem> items);

    Rule replaceSymbol(
        const Rule& rule,
        size_t item_index,
        const SymbolPtr& replacement_symbol
    );
    Rule replaceFirstSymbol(const Rule& rule, const SymbolPtr& replacement_symbol);
    Rule replaceFirstSymbolWithRule(const Rule& rule, const Rule& replacement_rule);

    Rule collapseItem(
        const Rule& rule,
        size_t item_index,
        ActionTreeNodePtr item_action,
        std::optional<std::string> expanded_symbol_name = std::nullopt
    );
    Rule collapseFirstItem(
        const Rule& rule,
        ActionTreeNodePtr item_action,
        std::optional<std::string> expanded_symbol_name = std::nullopt
    );

    std::optional<std::string> firstSymbolItemName(const Rule& rule);

    std::optional<SymbolPtr> firstSymbolItem(
        const SymbolsStorage& symbols_storage,
        const Rule& rule
    );

    bool isExactSymbolReference(
        const spec::RuleItemSymbol& item_symbol,
        const SymbolPtr& symbol
    );

    bool startsWithSymbol(const Rule& rule, const std::string& symbol_name);
    bool startsWithSymbol(const Rule& rule, const SymbolPtr& symbol);
    bool startsWithExactSymbol(const Rule& rule, const SymbolPtr& symbol);

} // namespace mparse::analysis
