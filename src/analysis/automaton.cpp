#include "automaton.h"

#include <analysis/symbols_storage.h>
#include <analysis/utils/rules.h>
#include <mparse/utils.h>

#include <ranges>
#include <utility>

namespace mparse::analysis {

    namespace {

        std::vector<SymbolPtr> collectAutomataSymbols(
            const std::vector<SymbolAutomaton>& automata
        ) {
            std::vector<SymbolPtr> symbols;
            symbols.reserve(automata.size());

            for (const auto& automaton : automata) {
                symbols.push_back(VERIFY(automaton.symbol));
            }

            return symbols;
        }

        AutomatonVertexId appendVertex(SymbolAutomaton& automaton) {
            automaton.vertices.emplace_back();
            return automaton.vertices.size() - 1;
        }

        void appendEdge(
            SymbolAutomaton& automaton,
            AutomatonVertexId from,
            AutomatonVertexId target,
            AutomatonTransition transition
        ) {
            automaton.vertices.at(from).edges.push_back(AutomatonEdge{
                .target = target,
                .transition = std::move(transition),
            });
        }

        std::string symbolValueType(const SymbolPtr& symbol) {
            VERIFY(symbol);
            return symbol->type().value_or("std::monostate");
        }

        std::string itemValueType(
            const spec::RuleItem& item,
            const SymbolsStorage& symbols_storage
        ) {
            return std::visit(
                mparse::Overloaded{
                    [](const spec::RuleItemLiteral&) {
                        return std::string{"std::string"};
                    },
                    [](const spec::RuleItemRange&) {
                        return std::string{"char"};
                    },
                    [&](const spec::RuleItemSymbol& item_symbol) {
                        return symbolValueType(
                            symbols_storage.getSymbolByName(item_symbol.name)
                        );
                    },
                },
                item.value
            );
        }

        std::vector<std::string> ruleArgumentTypes(
            const Rule& rule,
            const SymbolsStorage& symbols_storage
        ) {
            std::vector<std::string> types;
            types.reserve(rule.items().size());

            for (const auto& item : rule.items()) {
                types.push_back(itemValueType(item, symbols_storage));
            }

            return types;
        }

        AutomatonTransition makeTransition(
            const spec::RuleItem& item,
            const SymbolsStorage& symbols_storage
        ) {
            return std::visit(
                mparse::Overloaded{
                    [](const spec::RuleItemLiteral& literal) -> AutomatonTransition {
                        return LiteralTransition{
                            .value = literal.value,
                        };
                    },
                    [](const spec::RuleItemRange& range) -> AutomatonTransition {
                        return RangeTransition{
                            .from = range.from,
                            .to = range.to,
                        };
                    },
                    [&](const spec::RuleItemSymbol& item_symbol) -> AutomatonTransition {
                        return SymbolTransition{
                            .symbol = symbols_storage.getSymbolByName(item_symbol.name),
                            .arguments = item_symbol.arguments,
                        };
                    },
                },
                item.value
            );
        }

        void appendRulePath(
            SymbolAutomaton& automaton,
            const Rule& rule,
            const SymbolsStorage& symbols_storage
        ) {
            // TODO: merge common prefixes only after determinism analysis proves
            // that sharing a prefix does not change priority/order semantics.
            const auto self_starting_rule =
                startsWithExactSymbol(rule, automaton.symbol);
            auto current = self_starting_rule ? automaton.end : automaton.start;
            const size_t first_item_index = self_starting_rule ? 1 : 0;

            for (const auto& item : rule.items() | std::views::drop(first_item_index)) {
                const auto next = appendVertex(automaton);
                appendEdge(
                    automaton,
                    current,
                    next,
                    makeTransition(item, symbols_storage)
                );
                current = next;
            }

            appendEdge(
                automaton,
                current,
                automaton.end,
                ReduceTransition{
                    .action = VERIFY(rule.action()),
                    .result_type = symbolValueType(automaton.symbol),
                    .argument_types = ruleArgumentTypes(rule, symbols_storage),
                }
            );
        }

        SymbolAutomaton buildAutomaton(
            const SymbolPtr& symbol,
            const SymbolsStorage& symbols_storage
        ) {
            SymbolAutomaton automaton{
                .symbol = symbol,
                .start = 0,
                .end = 1,
                .vertices = {AutomatonVertex{}, AutomatonVertex{}},
            };

            for (const auto& rule : VERIFY(symbol)->rules()) {
                appendRulePath(automaton, rule, symbols_storage);
            }

            return automaton;
        }

    } // namespace

    GrammarAutomata::GrammarAutomata(std::vector<SymbolAutomaton> automata)
        : automata_(std::move(automata))
        , automaton_index_(collectAutomataSymbols(automata_)) {
        for (size_t index = 0; index < automata_.size(); index++) {
            automaton_index_[automata_[index].symbol] = index;
        }
    }

    const std::vector<SymbolAutomaton>& GrammarAutomata::symbols() const {
        return automata_;
    }

    const SymbolAutomaton& GrammarAutomata::getAutomaton(const SymbolPtr& symbol) const {
        return automata_.at(automaton_index_[symbol]);
    }

    const SymbolAutomaton& GrammarAutomata::getAutomatonByName(
        const std::string& name
    ) const {
        return automata_.at(automaton_index_[name]);
    }

    GrammarAutomata buildAutomata(const SymbolsStorage& symbols_storage) {
        std::vector<SymbolAutomaton> automata;
        const auto symbols = symbols_storage.symbols();
        automata.reserve(symbols.size());

        for (const auto& symbol : symbols) {
            automata.push_back(buildAutomaton(symbol, symbols_storage));
        }

        return GrammarAutomata(std::move(automata));
    }

} // namespace mparse::analysis
