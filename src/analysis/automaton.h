#pragma once

#include <analysis/rule.h>
#include <analysis/symbol.h>
#include <analysis/utils/symbols_info_array.h>

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace mparse::analysis {

    class SymbolsStorage;

    using AutomatonVertexId = size_t;

    struct LiteralTransition {
        std::string value;
    };

    struct RangeTransition {
        char from;
        char to;
    };

    struct SymbolTransition {
        SymbolPtr symbol;
        std::vector<std::string> arguments;
    };

    struct ReduceTransition {
        ActionTreeNodePtr action;
        std::string result_type;
        std::vector<std::string> argument_types;
    };

    using AutomatonTransition = std::variant<
        LiteralTransition,
        RangeTransition,
        SymbolTransition,
        ReduceTransition>;

    struct AutomatonEdge {
        AutomatonVertexId target = 0;
        AutomatonTransition transition;
    };

    struct AutomatonVertex {
        std::vector<AutomatonEdge> edges;
    };

    struct SymbolAutomaton {
        SymbolPtr symbol;
        AutomatonVertexId start = 0;
        AutomatonVertexId end = 1;
        std::vector<AutomatonVertex> vertices;
    };

    class GrammarAutomata {
    public:
        explicit GrammarAutomata(std::vector<SymbolAutomaton> automata);

        const std::vector<SymbolAutomaton>& symbols() const;
        const SymbolAutomaton& getAutomaton(const SymbolPtr& symbol) const;
        const SymbolAutomaton& getAutomatonByName(const std::string& name) const;

    private:
        std::vector<SymbolAutomaton> automata_;
        SymbolsInfoArray<size_t> automaton_index_;
    };

    GrammarAutomata buildAutomata(const SymbolsStorage& symbols_storage);

} // namespace mparse::analysis
