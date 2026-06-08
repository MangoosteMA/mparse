#include "expand_empty_items.h"

#include <analysis/algo/find_empty_symbols.h>
#include <analysis/error.h>
#include <analysis/rule.h>
#include <analysis/utils/rules.h>
#include <analysis/utils/symbols_info_array.h>
#include <mparse/utils.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace mparse::analysis {

    namespace {

        constexpr static auto kBeforeEmptyPartNameSuffix = "BeforeEmptyPart";
        constexpr static auto kAfterEmptyPartNameSuffix = "AfterEmptyPart";

        struct ExpandedSymbolInfo {
            std::optional<SymbolPtr> before_empty_part{};
            std::optional<SymbolPtr> after_empty_part{};
            ActionTreeNodePtr empty_rule_action{};

            bool canBeEmpty() const {
                return before_empty_part.has_value();
            }
        };

        using ExpandedSymbolsInfoArray = SymbolsInfoArray<ExpandedSymbolInfo>;

        void copySymbolsWithoutRulesTo(
            const std::vector<SymbolPtr>& symbols,
            SymbolsStorage& symbols_storage
        ) {
            for (const auto& symbol : symbols) {
                auto clone = Symbol::cloneWithoutRules(*symbol);
                symbols_storage.appendNewSymbol(std::move(clone));
            }
        }

        SymbolPtr insertSymbolPart(
            Symbol symbol,
            SymbolsStorage& expanded_symbols_storage,
            const std::string& name_suffix
        ) {
            const auto new_symbol_name = expanded_symbols_storage.generateUnexistingName(
                symbol.name() + name_suffix
            );

            auto clone = Symbol::cloneWithoutRules(std::move(symbol))
                             .setName(new_symbol_name);

            return expanded_symbols_storage.appendNewSymbol(std::move(clone));
        }

        struct SplitSymbolsResult {
            SymbolsStorage symbols_storage;
            ExpandedSymbolsInfoArray expanded_infos;
        };

        SplitSymbolsResult splitSymbols(
            const std::vector<SymbolPtr>& symbols,
            const SymbolsInfoArray<bool>& can_symbol_be_empty
        ) {
            SymbolsStorage expanded_symbols_storage;
            ExpandedSymbolsInfoArray expanded_infos(symbols);

            copySymbolsWithoutRulesTo(symbols, expanded_symbols_storage);

            for (const auto& symbol : symbols) {
                if (!can_symbol_be_empty[symbol]) {
                    continue;
                }
                auto& expanded_info = expanded_infos[symbol];

                expanded_info.before_empty_part = insertSymbolPart(
                    *symbol,
                    expanded_symbols_storage,
                    kBeforeEmptyPartNameSuffix
                );

                expanded_info.after_empty_part = insertSymbolPart(
                    *symbol,
                    expanded_symbols_storage,
                    kAfterEmptyPartNameSuffix
                );
            }

            return SplitSymbolsResult{
                .symbols_storage = std::move(expanded_symbols_storage),
                .expanded_infos = std::move(expanded_infos),
            };
        }

        bool canRuleItemBeEmpty(
            const spec::RuleItem& item,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            return std::visit(
                mparse::Overloaded{
                    [](const spec::RuleItemLiteral& literal) {
                        return literal.empty();
                    },
                    [](const spec::RuleItemRange& range) {
                        return range.empty();
                    },
                    [&](const spec::RuleItemSymbol& item_symbol) {
                        return expanded_infos[item_symbol.name].canBeEmpty();
                    },
                },
                item.value
            );
        }

        const spec::RuleItemSymbol* symbolItemAt(
            const Rule& rule,
            size_t item_index
        ) {
            const auto& items = rule.items();
            if (item_index >= items.size()) {
                return nullptr;
            }

            return std::get_if<spec::RuleItemSymbol>(&items[item_index].value);
        }

        bool hasSymbolAt(
            const Rule& rule,
            const SymbolPtr& symbol,
            size_t item_index
        ) {
            const auto* item_symbol = symbolItemAt(rule, item_index);
            return item_symbol && item_symbol->name == VERIFY(symbol)->name();
        }

        void throwIfUnsupportedSelfStartingRule(
            const Rule& rule,
            const SymbolPtr& symbol
        ) {
            if (startsWithSymbol(rule, symbol) && hasSymbolAt(rule, symbol, 1)) {
                throw makeUnsupportedSelfStartingRuleError(*VERIFY(symbol));
            }
        }

        std::vector<Rule> expandRuleFromItem(
            const Rule& rule,
            const ExpandedSymbolsInfoArray& expanded_infos,
            size_t item_index,
            const SymbolPtr& stop_symbol
        ) {
            const auto& items = rule.items();
            if (item_index >= items.size() ||
                !canRuleItemBeEmpty(items[item_index], expanded_infos)) {
                return {rule};
            }

            const auto* item_symbol = symbolItemAt(rule, item_index);
            if (!item_symbol || (stop_symbol && item_symbol->name == stop_symbol->name())) {
                return {rule};
            }

            const auto& expanded_info = expanded_infos[item_symbol->name];

            VERIFY(expanded_info.before_empty_part);
            VERIFY(expanded_info.after_empty_part);

            std::vector<Rule> expanded_rules;

            expanded_rules.push_back(
                replaceSymbol(rule, item_index, *expanded_info.before_empty_part)
            );

            for (auto&& expanded_rule : expandRuleFromItem(
                     collapseItem(
                         rule,
                         item_index,
                         ActionTreeNode::makeEmptySymbolReference(item_symbol->name),
                         item_symbol->name
                     ),
                     expanded_infos,
                     item_index,
                     stop_symbol
                 )) {
                expanded_rules.push_back(std::move(expanded_rule));
            }

            expanded_rules.push_back(
                replaceSymbol(rule, item_index, *expanded_info.after_empty_part)
            );

            return expanded_rules;
        }

        std::vector<Rule> expandRule(
            const Rule& rule,
            const SymbolPtr& symbol,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            if (startsWithSymbol(rule, symbol)) {
                return {rule};
            }

            return expandRuleFromItem(rule, expanded_infos, 0, symbol);
        }

        std::vector<Rule> expandRulePrefixes(
            const SymbolPtr& symbol,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            std::vector<Rule> expanded_rules;
            for (const auto& rule : symbol->rules()) {
                for (auto&& expanded_rule : expandRule(rule, symbol, expanded_infos)) {
                    expanded_rules.push_back(std::move(expanded_rule));
                }
            }
            return expanded_rules;
        }

        std::vector<Rule> expandSelfStartingRule(
            const Rule& rule,
            const SymbolPtr& symbol,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            if (!startsWithSymbol(rule, symbol)) {
                return {rule};
            }

            throwIfUnsupportedSelfStartingRule(rule, symbol);

            auto expanded_rules = expandRuleFromItem(rule, expanded_infos, 1, nullptr);
            for (const auto& expanded_rule : expanded_rules) {
                throwIfUnsupportedSelfStartingRule(expanded_rule, symbol);
            }

            return expanded_rules;
        }

        std::vector<Rule> expandSelfStartingRules(
            const SymbolPtr& symbol,
            const std::vector<Rule>& rules,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            std::vector<Rule> expanded_rules;
            for (const auto& rule : rules) {
                for (auto&& expanded_rule : expandSelfStartingRule(
                         rule,
                         symbol,
                         expanded_infos
                     )) {
                    expanded_rules.push_back(std::move(expanded_rule));
                }
            }
            return expanded_rules;
        }

        std::vector<Rule> expandRules(
            const SymbolPtr& symbol,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            return expandSelfStartingRules(
                symbol,
                expandRulePrefixes(symbol, expanded_infos),
                expanded_infos
            );
        }

        struct EmptyPartRules {
            std::vector<Rule> before_empty_rules;
            ActionTreeNodePtr empty_rule_action;
            std::vector<Rule> after_empty_rules;
            std::vector<Rule> self_starting_rules;
        };

        EmptyPartRules splitEmptyPartRules(
            const SymbolPtr& symbol,
            std::vector<Rule> expanded_rules
        ) {
            EmptyPartRules result;

            for (auto&& expanded_rule : expanded_rules) {
                if (expanded_rule.empty()) {
                    if (!result.empty_rule_action) {
                        result.empty_rule_action = expanded_rule.action();
                    }
                    continue;
                }

                if (startsWithSymbol(expanded_rule, symbol)) {
                    result.self_starting_rules.push_back(std::move(expanded_rule));
                    continue;
                }

                if (!result.empty_rule_action) {
                    result.before_empty_rules.push_back(std::move(expanded_rule));
                } else {
                    result.after_empty_rules.push_back(std::move(expanded_rule));
                }
            }

            VERIFY(result.empty_rule_action);
            return result;
        }

        using EmptyRuleActions = SymbolsInfoArray<ActionTreeNodePtr>;
        using ResolvedActionCache = std::unordered_map<const ActionTreeNode*, ActionTreeNodePtr>;

        ActionTreeNodePtr resolveEmptySymbolReferences(
            const ActionTreeNodePtr& action,
            const EmptyRuleActions& empty_rule_actions,
            ResolvedActionCache& resolved_actions
        ) {
            VERIFY(action);

            if (action->kind() == ActionTreeNodeKind::EmptySymbolReference) {
                return resolveEmptySymbolReferences(
                    empty_rule_actions[*VERIFY(action->symbolName())],
                    empty_rule_actions,
                    resolved_actions
                );
            }

            const auto [iterator, inserted] =
                resolved_actions.emplace(action.get(), ActionTreeNodePtr{});
            if (!inserted) {
                return iterator->second;
            }

            auto resolved_action = action->cloneWithoutEdges();
            iterator->second = resolved_action;

            std::vector<ActionTreeEdge> resolved_edges;
            resolved_edges.reserve(action->edges().size());

            for (const auto& edge : action->edges()) {
                resolved_edges.push_back(ActionTreeEdge{
                    .node = resolveEmptySymbolReferences(edge.node, empty_rule_actions, resolved_actions),
                    .offset_left = edge.offset_left,
                    .offset_right = edge.offset_right,
                    .expanded_symbol_name = edge.expanded_symbol_name,
                });
            }

            resolved_action->setEdges(std::move(resolved_edges));
            return resolved_action;
        }

        std::vector<Rule> resolveEmptySymbolReferences(
            const std::vector<Rule>& rules,
            const EmptyRuleActions& empty_rule_actions,
            ResolvedActionCache& resolved_actions
        ) {
            std::vector<Rule> resolved_rules;
            resolved_rules.reserve(rules.size());

            for (const auto& rule : rules) {
                resolved_rules.push_back(Rule(
                    rule.items(),
                    resolveEmptySymbolReferences(
                        rule.action(),
                        empty_rule_actions,
                        resolved_actions
                    )
                ));
            }

            return resolved_rules;
        }

        EmptyRuleActions collectEmptyRuleActions(
            const std::vector<SymbolPtr>& original_symbols,
            const SymbolsInfoArray<bool>& can_symbol_be_empty,
            const ExpandedSymbolsInfoArray& expanded_infos
        ) {
            EmptyRuleActions empty_rule_actions(original_symbols);

            for (const auto& symbol : original_symbols) {
                if (can_symbol_be_empty[symbol]) {
                    empty_rule_actions[symbol] = VERIFY(expanded_infos[symbol].empty_rule_action);
                }
            }

            return empty_rule_actions;
        }

        void resolveEmptySymbolReferences(
            SymbolsStorage& expanded_symbols_storage,
            const EmptyRuleActions& empty_rule_actions
        ) {
            ResolvedActionCache resolved_actions;

            for (const auto& symbol : expanded_symbols_storage.symbols()) {
                symbol->setRules(resolveEmptySymbolReferences(
                    symbol->rules(),
                    empty_rule_actions,
                    resolved_actions
                ));
            }
        }

    } // namespace

    SymbolsStorage expandEmptyItems(const SymbolsStorage& symbols_storage) {
        const auto can_symbol_be_empty = findEmptySymbols(symbols_storage);

        const auto original_symbols = symbols_storage.symbols();
        auto [expanded_symbols_storage, expanded_infos] = splitSymbols(
            original_symbols,
            can_symbol_be_empty
        );

        for (const auto& symbol : original_symbols) {
            auto& expanded_info = expanded_infos[symbol];
            auto expanded_rules = expandRules(symbol, expanded_infos);

            if (!can_symbol_be_empty[symbol]) {
                expanded_symbols_storage.getSymbolByName(symbol->name())
                    ->setRules(std::move(expanded_rules));
                continue;
            }

            VERIFY(expanded_info.before_empty_part);
            VERIFY(expanded_info.after_empty_part);

            auto [before_empty_rules, empty_rule_action, after_empty_rules, self_starting_rules] =
                splitEmptyPartRules(symbol, std::move(expanded_rules));

            expanded_info.empty_rule_action = empty_rule_action;

            expanded_info.before_empty_part.value()->setRules(std::move(before_empty_rules));
            expanded_info.after_empty_part.value()->setRules(std::move(after_empty_rules));

            std::vector<Rule> original_symbol_rules{
                makeForwardSemanticValueRule(expanded_info.before_empty_part.value()),
                Rule::makeEmpty(expanded_info.empty_rule_action),
                makeForwardSemanticValueRule(expanded_info.after_empty_part.value()),
            };

            for (auto&& self_starting_rule : self_starting_rules) {
                original_symbol_rules.push_back(std::move(self_starting_rule));
            }

            expanded_symbols_storage.getSymbolByName(symbol->name())
                ->setRules(std::move(original_symbol_rules));
        }

        resolveEmptySymbolReferences(
            expanded_symbols_storage,
            collectEmptyRuleActions(
                original_symbols,
                can_symbol_be_empty,
                expanded_infos
            )
        );

        return expanded_symbols_storage;
    }

} // namespace mparse::analysis
