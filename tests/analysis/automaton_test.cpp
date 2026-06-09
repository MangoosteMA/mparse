#include <analysis/automaton.h>
#include <analysis/analyze.h>
#include <analysis/symbols_storage.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

    const mparse::analysis::AutomatonEdge& onlyEdge(
        const mparse::analysis::SymbolAutomaton& automaton,
        mparse::analysis::AutomatonVertexId vertex
    ) {
        const auto& edges = automaton.vertices.at(vertex).edges;
        EXPECT_EQ(edges.size(), 1);
        return edges.front();
    }

    template <typename Transition>
    const Transition& asTransition(const mparse::analysis::AutomatonEdge& edge) {
        return std::get<Transition>(edge.transition);
    }

    mparse::spec::RuleItem symbolItem(
        std::string name,
        std::vector<std::string> arguments
    ) {
        return mparse::spec::RuleItem{
            .value = mparse::spec::RuleItemSymbol{
                .name = std::move(name),
                .arguments = std::move(arguments),
            },
        };
    }

} // namespace

TEST(Automaton, BuildsRulePathWithDedicatedReduceTransition) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("B", {
                        rule({literalItem("b")}, "$$ = $1"),
                    }),
        symbol("S", {
                        rule({literalItem("a"), rangeItem('0', '9'), symbolItem("B")}, "$$ = makeS($1, $2, $3)"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("S");

    ASSERT_EQ(automaton.vertices.size(), 5);

    const auto& literal_edge = onlyEdge(automaton, automaton.start);
    EXPECT_NE(literal_edge.target, automaton.end);
    EXPECT_EQ(asTransition<mparse::analysis::LiteralTransition>(literal_edge).value, "a");

    const auto& range_edge = onlyEdge(automaton, literal_edge.target);
    EXPECT_NE(range_edge.target, automaton.end);
    const auto& range = asTransition<mparse::analysis::RangeTransition>(range_edge);
    EXPECT_EQ(range.from, '0');
    EXPECT_EQ(range.to, '9');

    const auto& symbol_edge = onlyEdge(automaton, range_edge.target);
    EXPECT_NE(symbol_edge.target, automaton.end);
    const auto& symbol = asTransition<mparse::analysis::SymbolTransition>(symbol_edge);
    EXPECT_EQ(symbol.symbol->name(), "B");
    EXPECT_TRUE(symbol.arguments.empty());

    const auto& reduce_edge = onlyEdge(automaton, symbol_edge.target);
    EXPECT_EQ(reduce_edge.target, automaton.end);
    EXPECT_TRUE(std::holds_alternative<mparse::analysis::ReduceTransition>(
        reduce_edge.transition
    ));
}

TEST(Automaton, BuildsEmptyRuleAsReduceTransitionToEnd) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("S", {
                        emptyRule("$$ = makeS()"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("S");

    ASSERT_EQ(automaton.vertices.size(), 2);

    const auto& edge = onlyEdge(automaton, automaton.start);
    EXPECT_EQ(edge.target, automaton.end);
    const auto& reduce = asTransition<mparse::analysis::ReduceTransition>(edge);
    ASSERT_TRUE(reduce.action);
    EXPECT_TRUE(reduce.action->action().has_value());
    EXPECT_EQ(*reduce.action->action(), "$$ = makeS()");
}

TEST(Automaton, BuildsRepeatedLiteralTransition) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "S",
                .type = "std::string",
                .arguments = {
                    mparse::spec::SymbolArgument{
                        .name = "offset",
                        .type = "int",
                    },
                },
                .rules = {
                    mparse::tests::rule(
                        {mparse::tests::repeatedLiteralItem(" ", "offset")},
                        "$$ = $1"
                    ),
                },
            },
        },
    };

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("S");

    ASSERT_EQ(automaton.vertices.size(), 3);

    const auto& repeated_edge = onlyEdge(automaton, automaton.start);
    const auto& repeated =
        asTransition<mparse::analysis::RepeatedLiteralTransition>(repeated_edge);
    EXPECT_EQ(repeated.value, " ");
    EXPECT_EQ(repeated.count_expression, "offset");

    const auto& reduce_edge = onlyEdge(automaton, repeated_edge.target);
    const auto& reduce = asTransition<mparse::analysis::ReduceTransition>(reduce_edge);
    ASSERT_EQ(reduce.argument_types.size(), 1);
    EXPECT_EQ(reduce.argument_types.front(), "std::string");
}

TEST(Automaton, BuildsExactSelfRecursiveRuleFromEndToEnd) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("A", {
                        emptyRule("$$ = makeEmpty()"),
                        rule({symbolItem("A"), literalItem("x")}, "$$ = append($1, $2)"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("A");

    const auto& empty_reduce = onlyEdge(automaton, automaton.start);
    EXPECT_EQ(empty_reduce.target, automaton.end);
    EXPECT_TRUE(std::holds_alternative<mparse::analysis::ReduceTransition>(
        empty_reduce.transition
    ));

    const auto& recursive_literal = onlyEdge(automaton, automaton.end);
    EXPECT_NE(recursive_literal.target, automaton.end);
    EXPECT_EQ(
        asTransition<mparse::analysis::LiteralTransition>(recursive_literal).value,
        "x"
    );

    const auto& recursive_reduce = onlyEdge(automaton, recursive_literal.target);
    EXPECT_EQ(recursive_reduce.target, automaton.end);
    EXPECT_TRUE(std::holds_alternative<mparse::analysis::ReduceTransition>(
        recursive_reduce.transition
    ));
}

TEST(Automaton, DoesNotTreatChangedArgumentsAsExactSelfReference) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "A",
                .arguments = {
                    mparse::spec::SymbolArgument{
                        .name = "offset",
                        .type = "int",
                    },
                },
                .rules = {
                    mparse::tests::rule(
                        {symbolItem("A", {"offset + 1"}), mparse::tests::literalItem("x")},
                        "$$ = $2"
                    ),
                },
            },
        },
    };

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("A");

    EXPECT_TRUE(automaton.vertices.at(automaton.end).edges.empty());

    const auto& self_call = onlyEdge(automaton, automaton.start);
    const auto& symbol = asTransition<mparse::analysis::SymbolTransition>(self_call);
    EXPECT_EQ(symbol.symbol->name(), "A");
    ASSERT_EQ(symbol.arguments.size(), 1);
    EXPECT_EQ(symbol.arguments.front(), "offset + 1");
}

TEST(Automaton, BuildsAutomataFromAnalyzeResult) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("S", {
                        rule({literalItem("a")}, "$$ = $1"),
                    }),
    });

    const auto symbols_storage = mparse::analysis::analyzeOrThrow(specification);
    const auto automata = symbols_storage.buildAutomata();
    const auto& automaton = automata.getAutomatonByName("S");

    const auto& literal = onlyEdge(automaton, automaton.start);
    EXPECT_NE(literal.target, automaton.end);
    EXPECT_EQ(asTransition<mparse::analysis::LiteralTransition>(literal).value, "a");

    const auto& reduce = onlyEdge(automaton, literal.target);
    EXPECT_EQ(reduce.target, automaton.end);
    EXPECT_TRUE(std::holds_alternative<mparse::analysis::ReduceTransition>(
        reduce.transition
    ));
}
