#include "statistics.h"

#include <analysis/utils/symbols_info_array.h>
#include <mparse/utils.h>

#include <optional>
#include <ostream>
#include <utility>

namespace mparse::analysis {

    namespace {

        SymbolStatistics makeSymbolStatistics(
            const SymbolPtr& symbol,
            GrammarStatisticsMode mode
        ) {
            if (mode == GrammarStatisticsMode::OriginalSymbols) {
                const auto& original_symbol = symbol->originalSymbol();
                return SymbolStatistics{
                    .name = original_symbol.name,
                    .source_reference = original_symbol.source_reference,
                    .rules = 0,
                    .items = 0,
                    .rule_statistics = {},
                };
            }

            return SymbolStatistics{
                .name = symbol->name(),
                .source_reference = symbol->sourceReference(),
                .rules = 0,
                .items = 0,
                .rule_statistics = {},
            };
        }

        void addSymbolRules(
            SymbolStatistics& symbol_statistics,
            const SymbolPtr& symbol
        ) {
            symbol_statistics.rules += symbol->rules().size();

            for (const auto& rule : symbol->rules()) {
                const auto items = rule.items().size();

                symbol_statistics.items += items;
                symbol_statistics.rule_statistics.push_back(RuleStatistics{
                    .items = items,
                });
            }
        }

        void addSymbolStatistics(
            GrammarStatistics& grammar_statistics,
            SymbolStatistics symbol_statistics
        ) {
            grammar_statistics.symbols++;
            grammar_statistics.rules += symbol_statistics.rules;
            grammar_statistics.items += symbol_statistics.items;
            grammar_statistics.symbol_statistics.push_back(std::move(symbol_statistics));
        }

        GrammarStatistics collectExpandedGrammarStatistics(
            const SymbolsStorage& symbols_storage
        ) {
            GrammarStatistics statistics;

            for (const auto& symbol : symbols_storage.symbols()) {
                auto symbol_statistics = makeSymbolStatistics(
                    symbol,
                    GrammarStatisticsMode::ExpandedSymbols
                );
                symbol_statistics.rule_statistics.reserve(symbol->rules().size());
                addSymbolRules(symbol_statistics, symbol);
                addSymbolStatistics(statistics, std::move(symbol_statistics));
            }

            return statistics;
        }

        GrammarStatistics collectOriginalGrammarStatistics(
            const SymbolsStorage& symbols_storage
        ) {
            const auto symbols = symbols_storage.symbols();

            GrammarStatistics statistics;
            SymbolsInfoArray<std::optional<size_t>> original_symbol_to_statistics(symbols);

            for (const auto& symbol : symbols) {
                if (symbol->name() != symbol->originalName()) {
                    continue;
                }

                original_symbol_to_statistics[symbol] = statistics.symbol_statistics.size();
                statistics.symbol_statistics.push_back(
                    makeSymbolStatistics(symbol, GrammarStatisticsMode::OriginalSymbols)
                );
            }

            for (const auto& symbol : symbols) {
                const auto statistics_index =
                    *VERIFY(original_symbol_to_statistics[symbol->originalName()]);
                addSymbolRules(statistics.symbol_statistics[statistics_index], symbol);
            }

            for (const auto& symbol_statistics : statistics.symbol_statistics) {
                statistics.symbols++;
                statistics.rules += symbol_statistics.rules;
                statistics.items += symbol_statistics.items;
            }

            return statistics;
        }

    } // namespace

    std::string SymbolStatistics::formatWithSourceReference() const {
        return spec::formatWithSourceReference(name, source_reference);
    }

    GrammarStatistics collectGrammarStatistics(
        const SymbolsStorage& symbols_storage,
        GrammarStatisticsMode mode
    ) {
        if (mode == GrammarStatisticsMode::ExpandedSymbols) {
            return collectExpandedGrammarStatistics(symbols_storage);
        }

        return collectOriginalGrammarStatistics(symbols_storage);
    }

    void printGrammarStatistics(
        const SymbolsStorage& symbols_storage,
        std::ostream& output,
        int verbosity
    ) {
        if (verbosity <= 0) {
            return;
        }

        const auto mode = verbosity >= 3
                              ? GrammarStatisticsMode::ExpandedSymbols
                              : GrammarStatisticsMode::OriginalSymbols;

        const auto statistics = collectGrammarStatistics(symbols_storage, mode);

        if (mode == GrammarStatisticsMode::ExpandedSymbols) {
            output << "expanded grammar statistics:\n";
        } else {
            output << "grammar statistics:\n";
        }

        output << "  symbols: " << statistics.symbols << '\n';
        output << "  rules: " << statistics.rules << '\n';
        output << "  items: " << statistics.items << '\n';

        if (verbosity <= 1) {
            return;
        }

        output << "symbol statistics:\n";
        for (const auto& symbol_statistics : statistics.symbol_statistics) {
            output << "  " << symbol_statistics.formatWithSourceReference()
                   << ": rules=" << symbol_statistics.rules
                   << ", items=" << symbol_statistics.items << '\n';

            for (size_t rule_index = 0;
                 rule_index < symbol_statistics.rule_statistics.size();
                 rule_index++) {
                output << "    rule " << rule_index
                       << ": items="
                       << symbol_statistics.rule_statistics[rule_index].items
                       << '\n';
            }
        }
    }

} // namespace mparse::analysis
