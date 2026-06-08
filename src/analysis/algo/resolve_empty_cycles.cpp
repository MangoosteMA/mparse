#include "resolve_empty_cycles.h"

#include <analysis/utils/rules.h>
#include <analysis/utils/symbol_rules.h>
#include <analysis/utils/symbols_info_array.h>
#include <mparse/strongly_connected_components.h>
#include <mparse/utils.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    namespace {

        enum class VisitState {
            Unvisited,
            Visiting,
            Visited,
        };

        struct FirstSymbolEdge {
            size_t to;
            ActionTreeNodePtr action;
        };

        struct FirstSymbolGraph {
            std::vector<SymbolPtr> symbols;
            SymbolsInfoArray<size_t> symbol_to_index;
            std::vector<std::vector<FirstSymbolEdge>> edges;
            std::vector<bool> has_self_loop;

            size_t size() const {
                return symbols.size();
            }
        };

        struct StronglyConnectedComponent {
            size_t id = 0;
            std::vector<size_t> vertices;
            size_t self_loops = 0;
        };

        struct StronglyConnectedComponentsInfo {
            std::vector<size_t> vertex_to_component;
            std::vector<StronglyConnectedComponent> components;
        };

        void collectFirstItemExpandedSymbolNames(
            const ActionTreeNodePtr& action,
            std::vector<std::string>& expanded_symbol_names
        ) {
            if (!action) {
                return;
            }

            for (const auto& edge : action->edges()) {
                if (edge.offset_left > 0) {
                    break;
                }

                if (edge.expanded_symbol_name) {
                    expanded_symbol_names.push_back(*edge.expanded_symbol_name);
                }

                if (edge.offset_right > 0) {
                    collectFirstItemExpandedSymbolNames(edge.node, expanded_symbol_names);
                    break;
                }
            }
        }

        FirstSymbolGraph buildFirstSymbolGraph(const SymbolsStorage& symbols_storage) {
            auto symbols = symbols_storage.symbols();
            SymbolsInfoArray<size_t> symbol_to_index(symbols);

            FirstSymbolGraph graph{
                .symbols = std::move(symbols),
                .symbol_to_index = std::move(symbol_to_index),
                .edges = {},
                .has_self_loop = {},
            };

            graph.edges.resize(graph.size());
            graph.has_self_loop.assign(graph.size(), false);

            for (size_t index = 0; index < graph.size(); index++) {
                graph.symbol_to_index[graph.symbols[index]] = index;
            }

            for (size_t symbol_index = 0; symbol_index < graph.size(); symbol_index++) {
                const auto& symbol = graph.symbols[symbol_index];
                auto& edges = graph.edges[symbol_index];

                for (const auto& rule : symbol->rules()) {
                    const auto next_symbol = firstSymbolItem(symbols_storage, rule);
                    if (!next_symbol) {
                        continue;
                    }

                    const auto next_symbol_index = graph.symbol_to_index[*next_symbol];
                    if (next_symbol_index == symbol_index) {
                        graph.has_self_loop[symbol_index] = true;
                        continue;
                    }

                    FirstSymbolEdge edge{
                        .to = next_symbol_index,
                        .action = rule.action(),
                    };

                    edges.push_back(std::move(edge));
                }
            }

            return graph;
        }

        std::vector<size_t> findSccs(const FirstSymbolGraph& graph) {
            mparse::StronglyConnectedComponents sccs(graph.size());

            for (size_t from = 0; from < graph.size(); from++) {
                for (const auto& edge : graph.edges[from]) {
                    sccs.addEdge(from, edge.to);
                }
            }

            return sccs.solve();
        }

        StronglyConnectedComponentsInfo findStronglyConnectedComponents(
            const FirstSymbolGraph& graph
        ) {
            auto vertex_to_component = findSccs(graph);

            size_t components_count = 0;
            for (const auto component_id : vertex_to_component) {
                components_count = std::max(components_count, component_id + 1);
            }

            std::vector<StronglyConnectedComponent> components;
            components.reserve(components_count);
            for (size_t component_id = 0; component_id < components_count; component_id++) {
                components.push_back(StronglyConnectedComponent{
                    .id = component_id,
                    .vertices = {},
                    .self_loops = 0,
                });
            }

            for (size_t vertex = 0; vertex < graph.size(); vertex++) {
                auto& component = components[vertex_to_component[vertex]];
                component.vertices.push_back(vertex);
                if (graph.has_self_loop[vertex]) {
                    component.self_loops++;
                }
            }

            std::ranges::sort(components, [](const auto& lhs, const auto& rhs) {
                return lhs.vertices < rhs.vertices;
            });

            return StronglyConnectedComponentsInfo{
                .vertex_to_component = std::move(vertex_to_component),
                .components = std::move(components),
            };
        }

        struct InlineCandidate {
            SymbolPtr source;
            SymbolPtr target;
        };

        struct ResolveAction {
            InlineCandidate inline_candidate;
        };

        std::optional<InlineCandidate> findInlineCandidate(
            const FirstSymbolGraph& graph,
            const StronglyConnectedComponentsInfo& components_info,
            const StronglyConnectedComponent& component
        ) {
            for (const auto from : component.vertices) {
                for (const auto& edge : graph.edges[from]) {
                    if (components_info.vertex_to_component[edge.to] != component.id) {
                        continue;
                    }

                    if (graph.has_self_loop[edge.to]) {
                        continue;
                    }

                    const auto& source = graph.symbols[from];
                    const auto& target = graph.symbols[edge.to];
                    if (!hasInlinableRules(source, target)) {
                        continue;
                    }

                    return InlineCandidate{
                        .source = source,
                        .target = target,
                    };
                }
            }

            return std::nullopt;
        }

        std::optional<ResolveAction> resolveComponent(
            const FirstSymbolGraph& graph,
            const StronglyConnectedComponentsInfo& components_info,
            const StronglyConnectedComponent& component
        ) {
            if (auto inline_candidate = findInlineCandidate(graph, components_info, component)) {
                return ResolveAction{
                    .inline_candidate = *inline_candidate,
                };
            }

            return std::nullopt;
        }

        std::optional<ResolveAction> resolveNextComponent(const FirstSymbolGraph& graph) {
            const auto components_info = findStronglyConnectedComponents(graph);

            for (const auto& component : components_info.components) {
                if (auto action = resolveComponent(graph, components_info, component)) {
                    return action;
                }
            }

            return std::nullopt;
        }

        void applyResolveAction(const ResolveAction& action) {
            action.inline_candidate.source->setRules(
                inlineFirstSymbolRules(
                    action.inline_candidate.source,
                    action.inline_candidate.target
                )
            );
        }

        struct CycleSearchInfo {
            VisitState visit_state = VisitState::Unvisited;
            std::optional<size_t> stack_position;
        };

        SymbolPtr originalSymbol(
            const FirstSymbolGraph& graph,
            const SymbolPtr& symbol
        ) {
            return graph.symbols[graph.symbol_to_index[VERIFY(symbol)->originalName()]];
        }

        void appendCycleSymbol(
            EmptyCycle& cycle,
            const FirstSymbolGraph& graph,
            const SymbolPtr& symbol
        ) {
            const auto original_symbol = originalSymbol(graph, symbol);
            if (!cycle.symbols.empty() && cycle.symbols.back() == original_symbol) {
                return;
            }

            cycle.symbols.push_back(original_symbol);
        }

        void appendRestoredEdge(
            EmptyCycle& cycle,
            const FirstSymbolGraph& graph,
            const FirstSymbolEdge& edge
        ) {
            std::vector<std::string> expanded_symbol_names;
            collectFirstItemExpandedSymbolNames(edge.action, expanded_symbol_names);

            for (const auto& expanded_symbol_name : expanded_symbol_names) {
                appendCycleSymbol(
                    cycle,
                    graph,
                    graph.symbols[graph.symbol_to_index[expanded_symbol_name]]
                );
            }
            appendCycleSymbol(cycle, graph, graph.symbols[edge.to]);
        }

        EmptyCycle makeCycle(
            const FirstSymbolGraph& graph,
            const std::vector<size_t>& stack,
            const std::vector<FirstSymbolEdge>& stack_edges,
            const std::vector<CycleSearchInfo>& infos,
            const FirstSymbolEdge& closing_edge
        ) {
            const auto cycle_start = *VERIFY(infos[closing_edge.to].stack_position);

            EmptyCycle cycle;
            appendCycleSymbol(cycle, graph, graph.symbols[stack[cycle_start]]);

            for (auto edge_index = cycle_start; edge_index < stack_edges.size(); edge_index++) {
                appendRestoredEdge(cycle, graph, stack_edges[edge_index]);
            }
            appendRestoredEdge(cycle, graph, closing_edge);

            return cycle;
        }

        std::optional<EmptyCycle> findUnresolvedCycleFrom(
            const FirstSymbolGraph& graph,
            size_t symbol_index,
            std::vector<CycleSearchInfo>& infos,
            std::vector<size_t>& stack,
            std::vector<FirstSymbolEdge>& stack_edges
        ) {
            auto& symbol_info = infos[symbol_index];
            symbol_info.visit_state = VisitState::Visiting;
            symbol_info.stack_position = stack.size();
            stack.push_back(symbol_index);

            for (const auto& edge : graph.edges[symbol_index]) {
                if (edge.to == symbol_index) {
                    continue;
                }

                auto& next_symbol_info = infos[edge.to];
                if (next_symbol_info.visit_state == VisitState::Visiting) {
                    return makeCycle(graph, stack, stack_edges, infos, edge);
                }

                if (next_symbol_info.visit_state == VisitState::Visited) {
                    continue;
                }

                stack_edges.push_back(edge);
                const auto cycle =
                    findUnresolvedCycleFrom(graph, edge.to, infos, stack, stack_edges);
                stack_edges.pop_back();

                if (cycle) {
                    return cycle;
                }
            }

            stack.pop_back();
            symbol_info.stack_position = std::nullopt;
            symbol_info.visit_state = VisitState::Visited;
            return std::nullopt;
        }

        std::optional<EmptyCycle> findUnresolvedCycle(const FirstSymbolGraph& graph) {
            std::vector<CycleSearchInfo> infos(graph.size());
            std::vector<size_t> stack;
            std::vector<FirstSymbolEdge> stack_edges;

            for (size_t symbol_index = 0; symbol_index < graph.size(); symbol_index++) {
                if (infos[symbol_index].visit_state != VisitState::Unvisited) {
                    continue;
                }

                if (auto cycle = findUnresolvedCycleFrom(
                        graph,
                        symbol_index,
                        infos,
                        stack,
                        stack_edges
                    )) {
                    return cycle;
                }
            }

            return std::nullopt;
        }

    } // namespace

    ResolveEmptyCyclesResult resolveEmptyCycles(SymbolsStorage symbols_storage) {
        while (true) {
            const auto graph = buildFirstSymbolGraph(symbols_storage);
            const auto action = resolveNextComponent(graph);
            if (!action) {
                break;
            }

            applyResolveAction(*action);
        }

        auto graph = buildFirstSymbolGraph(symbols_storage);

        return ResolveEmptyCyclesResult{
            .symbols_storage = std::move(symbols_storage),
            .unresolved_cycle = findUnresolvedCycle(graph),
        };
    }

    std::string formatEmptyCycle(const EmptyCycle& cycle) {
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
