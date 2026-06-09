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

    struct RepeatedLiteralTransition {
        std::string value;
        std::string count_expression;
    };

    struct RegexTransition {
        spec::RegexExpression expression;
    };

    struct SymbolTransition {
        SymbolPtr symbol;
        std::vector<std::string> arguments;
    };

    struct TextSemanticValue {
        bool operator==(const TextSemanticValue&) const = default;
    };

    struct CharacterSemanticValue {
        bool operator==(const CharacterSemanticValue&) const = default;
    };

    struct SymbolSemanticValue {
        SymbolPtr symbol;

        bool operator==(const SymbolSemanticValue&) const = default;
    };

    using SemanticValueType = std::variant<
        TextSemanticValue,
        CharacterSemanticValue,
        SymbolSemanticValue>;

    struct ReduceTransition {
        ActionTreeNodePtr action;
        SemanticValueType result_type;
        std::vector<SemanticValueType> argument_types;
    };

    using AutomatonTransition = std::variant<
        LiteralTransition,
        RangeTransition,
        RepeatedLiteralTransition,
        RegexTransition,
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
