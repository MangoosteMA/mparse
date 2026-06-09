#include "find_empty_end_cycles.h"

#include <analysis/algo/find_empty_symbols.h>
#include <analysis/utils/rules.h>
#include <analysis/utils/symbols_info_array.h>
#include <mparse/utils.h>

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace mparse::analysis {

    namespace {

        enum class VisitState {
            Unvisited,
            Visiting,
            Visited,
        };

        struct EmptyEndEdge {
            size_t to = 0;
        };

        struct EmptyEndGraph {
            std::vector<SymbolPtr> symbols;
            SymbolsInfoArray<size_t> symbol_to_index;
            std::vector<std::vector<EmptyEndEdge>> edges;

            size_t size() const {
                return symbols.size();
            }
        };

        struct CycleSearchInfo {
            VisitState visit_state = VisitState::Unvisited;
            std::optional<size_t> stack_position;
        };

        bool canRuleItemBeEmpty(
            const spec::RuleItem& item,
            const SymbolsInfoArray<bool>& can_symbol_be_empty
        ) {
            return std::visit(
                mparse::Overloaded{
                    [](const spec::RuleItemLiteral& literal) {
                        return literal.empty();
                    },
                    [](const spec::RuleItemRange& range) {
                        return range.empty();
                    },
                    [](const spec::RuleItemRepeatedLiteral& literal) {
                        return literal.empty();
                    },
                    [&](const spec::RuleItemSymbol& item_symbol) {
                        return can_symbol_be_empty[item_symbol.name];
                    },
                },
                item.value
            );
        }

        bool canSelfStartingTailBeEmpty(
            const Rule& rule,
            const SymbolsInfoArray<bool>& can_symbol_be_empty
        ) {
            for (auto item = std::next(rule.items().begin());
                 item != rule.items().end();
                 item++) {
                if (!canRuleItemBeEmpty(*item, can_symbol_be_empty)) {
                    return false;
                }
            }
            return true;
        }

        EmptyEndGraph buildEmptyEndGraph(const SymbolsStorage& symbols_storage) {
            auto symbols = symbols_storage.symbols();
            SymbolsInfoArray<size_t> symbol_to_index(symbols);
            const auto can_symbol_be_empty = findEmptySymbols(symbols_storage);

            EmptyEndGraph graph{
                .symbols = std::move(symbols),
                .symbol_to_index = std::move(symbol_to_index),
                .edges = {},
            };

            graph.edges.resize(graph.size());
            for (size_t index = 0; index < graph.size(); index++) {
                graph.symbol_to_index[graph.symbols[index]] = index;
            }

            for (size_t symbol_index = 0; symbol_index < graph.size(); symbol_index++) {
                const auto& symbol = graph.symbols[symbol_index];

                for (const auto& rule : symbol->rules()) {
                    if (!startsWithExactSymbol(rule, symbol)) {
                        continue;
                    }

                    if (!canSelfStartingTailBeEmpty(rule, can_symbol_be_empty)) {
                        continue;
                    }

                    graph.edges[symbol_index].push_back(EmptyEndEdge{
                        .to = symbol_index,
                    });
                }
            }

            return graph;
        }

        SymbolPtr originalSymbol(
            const EmptyEndGraph& graph,
            const SymbolPtr& symbol
        ) {
            return graph.symbols[graph.symbol_to_index[VERIFY(symbol)->originalName()]];
        }

        void appendCycleSymbol(
            EmptyEndCycle& cycle,
            const EmptyEndGraph& graph,
            const SymbolPtr& symbol
        ) {
            cycle.symbols.push_back(originalSymbol(graph, symbol));
        }

        EmptyEndCycle makeCycle(
            const EmptyEndGraph& graph,
            const std::vector<size_t>& stack,
            const std::vector<CycleSearchInfo>& infos,
            const EmptyEndEdge& closing_edge
        ) {
            const auto cycle_start = *VERIFY(infos[closing_edge.to].stack_position);

            EmptyEndCycle cycle;
            for (auto stack_index = cycle_start; stack_index < stack.size(); stack_index++) {
                appendCycleSymbol(cycle, graph, graph.symbols[stack[stack_index]]);
            }
            appendCycleSymbol(cycle, graph, graph.symbols[closing_edge.to]);

            return cycle;
        }

        std::optional<EmptyEndCycle> findEmptyEndCycleFrom(
            const EmptyEndGraph& graph,
            size_t symbol_index,
            std::vector<CycleSearchInfo>& infos,
            std::vector<size_t>& stack
        ) {
            auto& symbol_info = infos[symbol_index];
            symbol_info.visit_state = VisitState::Visiting;
            symbol_info.stack_position = stack.size();
            stack.push_back(symbol_index);

            for (const auto& edge : graph.edges[symbol_index]) {
                auto& next_symbol_info = infos[edge.to];
                if (next_symbol_info.visit_state == VisitState::Visiting) {
                    return makeCycle(graph, stack, infos, edge);
                }

                if (next_symbol_info.visit_state == VisitState::Visited) {
                    continue;
                }

                if (auto cycle = findEmptyEndCycleFrom(
                        graph,
                        edge.to,
                        infos,
                        stack
                    )) {
                    return cycle;
                }
            }

            stack.pop_back();
            symbol_info.stack_position = std::nullopt;
            symbol_info.visit_state = VisitState::Visited;
            return std::nullopt;
        }

    } // namespace

    std::optional<EmptyEndCycle> findEmptyEndCycle(
        const SymbolsStorage& symbols_storage
    ) {
        const auto graph = buildEmptyEndGraph(symbols_storage);
        std::vector<CycleSearchInfo> infos(graph.size());
        std::vector<size_t> stack;

        for (size_t symbol_index = 0; symbol_index < graph.size(); symbol_index++) {
            if (infos[symbol_index].visit_state != VisitState::Unvisited) {
                continue;
            }

            if (auto cycle = findEmptyEndCycleFrom(
                    graph,
                    symbol_index,
                    infos,
                    stack
                )) {
                return cycle;
            }
        }

        return std::nullopt;
    }

    std::string formatEmptyEndCycle(const EmptyEndCycle& cycle) {
        VERIFY(!cycle.symbols.empty());

        std::string result;
        for (const auto& symbol : cycle.symbols) {
            if (!result.empty()) {
                result += " -> ";
            }
            result += symbol->formatWithSourceReference();
        }
        return result;
    }

} // namespace mparse::analysis
