#include "find_empty_symbols.h"

#include <analysis/symbol.h>
#include <mparse/utils.h>

#include <queue>
#include <vector>

namespace mparse::analysis {

    namespace {

        struct Dependency {
            SymbolPtr symbol;
            size_t rule_index;
        };

        struct RuleInfo {
            bool might_be_empty = true;
            size_t unknown_items = 0;
        };

        struct SymbolInfo {
            bool can_be_empty = false;
            std::vector<RuleInfo> rule_infos{};
            std::vector<Dependency> dependencies{};
        };

        using InfosArray = SymbolsInfoArray<SymbolInfo>;

        void fillSymbolInfo(
            const SymbolPtr& symbol,
            InfosArray& infos,
            const SymbolsStorage& symbols_storage
        ) {
            const auto& rules = symbol->rules();

            auto& symbol_info = infos[symbol];
            symbol_info.rule_infos.resize(rules.size());

            for (size_t rule_index = 0; rule_index < rules.size(); rule_index++) {
                const auto& rule = rules[rule_index];
                auto& rule_info = symbol_info.rule_infos[rule_index];

                for (const auto& item : rule.items()) {
                    std::visit(
                        mparse::Overloaded{
                            [&](const spec::RuleItemLiteral& literal) {
                                rule_info.might_be_empty &= literal.empty();
                            },
                            [&](const spec::RuleItemRange& range) {
                                rule_info.might_be_empty &= range.empty();
                            },
                            [&](const spec::RuleItemRepeatedLiteral& literal) {
                                rule_info.might_be_empty &= literal.empty();
                            },
                            [&](const spec::RuleItemSymbol& item_symbol) {
                                const auto& current_symbol = symbols_storage.getSymbolByName(item_symbol.name);
                                if (current_symbol == symbol) {
                                    rule_info.might_be_empty = false;
                                    return;
                                }

                                rule_info.unknown_items++;

                                infos[current_symbol].dependencies.push_back(Dependency{
                                    .symbol = symbol,
                                    .rule_index = rule_index,
                                });
                            },
                        },
                        item.value
                    );
                }

                if (rule_info.might_be_empty && rule_info.unknown_items == 0) {
                    symbol_info.can_be_empty = true;
                }
            }
        }

        InfosArray makeInfosArray(const SymbolsStorage& symbols_storage) {
            const auto symbols = symbols_storage.symbols();
            InfosArray infos(symbols);
            for (const auto& symbol : symbols) {
                fillSymbolInfo(symbol, infos, symbols_storage);
            }
            return infos;
        }

        SymbolsInfoArray<bool> makeEmptySymbolsArray(
            const InfosArray& infos,
            const std::vector<SymbolPtr>& symbols
        ) {
            SymbolsInfoArray<bool> result(symbols);
            for (const auto& symbol : symbols) {
                result[symbol] = infos[symbol].can_be_empty;
            }
            return result;
        }

    } // namespace

    SymbolsInfoArray<bool> findEmptySymbols(const SymbolsStorage& symbols_storage) {
        const auto symbols = symbols_storage.symbols();
        auto infos = makeInfosArray(symbols_storage);

        std::queue<SymbolPtr> empty_symbols;
        for (const auto& symbol : symbols) {
            if (infos[symbol].can_be_empty) {
                empty_symbols.push(symbol);
            }
        }

        while (!empty_symbols.empty()) {
            auto symbol = std::move(empty_symbols.front());
            empty_symbols.pop();

            const auto& symbol_info = infos[symbol];
            for (const auto& dependency : symbol_info.dependencies) {
                auto& dependent_symbol_info = infos[dependency.symbol];
                if (dependent_symbol_info.can_be_empty) {
                    continue;
                }

                auto& current_rule_info = dependent_symbol_info.rule_infos.at(dependency.rule_index);
                current_rule_info.unknown_items--;

                if (!current_rule_info.might_be_empty ||
                    current_rule_info.unknown_items > 0) {
                    continue;
                }

                dependent_symbol_info.can_be_empty = true;
                empty_symbols.push(dependency.symbol);
            }
        }

        return makeEmptySymbolsArray(infos, symbols);
    }

} // namespace mparse::analysis
