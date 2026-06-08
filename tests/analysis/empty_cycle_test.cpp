#include <analysis/algo/expand_empty_items.h>
#include <analysis/algo/find_empty_end_cycles.h>
#include <analysis/algo/resolve_empty_cycles.h>
#include <analysis/analyze.h>
#include <analysis/error.h>
#include <analysis/symbols_storage.h>
#include <utils/canon.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

    const mparse::spec::RuleItemLiteral& asLiteralItem(const mparse::spec::RuleItem& item) {
        return std::get<mparse::spec::RuleItemLiteral>(item.value);
    }

    void expectMatchesCanon(
        const std::filesystem::path& relative_path,
        std::string_view actual
    ) {
        EXPECT_EQ(
            mparse::tests::readOrUpdateCanon(relative_path, actual),
            mparse::tests::canonicalizeText(actual)
        );
    }

    mparse::spec::Specification makeResolvableCycleSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("B"), literalItem("aboba")}, "$$ = makeA($1, $2)"),
                        }),
            symbol("B", {
                            rule({symbolItem("A"), literalItem("bobaba")}, "$$ = makeB($1, $2)"),
                            rule({literalItem("something")}, "$$ = makeSomething($1)"),
                        }),
        });
    }

    mparse::spec::Specification makeResolvableThreeSymbolCycleSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("B"), literalItem("a")}, "$$ = makeA($1, $2)"),
                        }),
            symbol("B", {
                            rule({symbolItem("C"), literalItem("b")}, "$$ = makeB($1, $2)"),
                        }),
            symbol("C", {
                            rule({symbolItem("A"), literalItem("c")}, "$$ = makeC($1, $2)"),
                            rule({literalItem("x")}, "$$ = makeX($1)"),
                        }),
        });
    }

    mparse::spec::Specification makeResolvableCycleWithOneSelfLoopSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("A"), literalItem("self")}, "$$ = appendA($1, $2)"),
                            rule({symbolItem("B"), literalItem("a")}, "$$ = makeA($1, $2)"),
                        }),
            symbol("B", {
                            rule({symbolItem("A"), literalItem("b")}, "$$ = makeB($1, $2)"),
                            rule({literalItem("x")}, "$$ = makeX($1)"),
                        }),
        });
    }

    mparse::spec::Specification makeUnresolvedCycleSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("A"), literalItem("a")}, "$$ = appendA($1, $2)"),
                            rule({symbolItem("B"), literalItem("aboba")}, "$$ = makeA($1, $2)"),
                        },
                   mparse::spec::SourceReference{.line = 10, .column = 2}),
            symbol("B", {
                            rule({symbolItem("B"), literalItem("b")}, "$$ = appendB($1, $2)"),
                            rule({symbolItem("A"), literalItem("bobaba")}, "$$ = makeB($1, $2)"),
                        },
                   mparse::spec::SourceReference{.line = 20, .column = 3}),
        });
    }

    mparse::analysis::SymbolsStorage makeCycleThroughSyntheticSymbolStorage() {
        using namespace mparse::tests;

        mparse::analysis::SymbolsStorage symbols_storage;

        const auto symbol_a = symbols_storage.appendNewSymbol(mparse::analysis::Symbol(mparse::spec::Symbol{
            .name = "A",
            .source_reference = mparse::spec::SourceReference{.line = 30, .column = 4},
            .arguments = {
                mparse::spec::SymbolArgument{
                    .name = "value",
                    .type = "AValue",
                },
            },
            .rules = {
                rule({symbolItem("ASynthetic")}),
            },
        }));

        auto synthetic_a = mparse::analysis::Symbol::cloneWithoutRules(*symbol_a);
        synthetic_a.setName("ASynthetic");
        synthetic_a.setRules({
            mparse::analysis::Rule(rule({symbolItem("B")})),
        });
        symbols_storage.appendNewSymbol(std::move(synthetic_a));

        symbols_storage.appendNewSymbol(mparse::analysis::Symbol(mparse::spec::Symbol{
            .name = "B",
            .source_reference = mparse::spec::SourceReference{.line = 40, .column = 5},
            .arguments = {
                mparse::spec::SymbolArgument{
                    .name = "value",
                    .type = "BValue",
                },
            },
            .rules = {
                rule({symbolItem("A")}),
            },
        }));

        return symbols_storage;
    }

    mparse::spec::Specification makeUnresolvedCycleThroughInlinedEdgesSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("B"), literalItem("a")}, "$$ = makeA($1, $2)"),
                        }),
            symbol("B", {
                            rule({symbolItem("C"), literalItem("b")}, "$$ = makeB($1, $2)"),
                        }),
            symbol("C", {
                            rule({symbolItem("C"), literalItem("c")}, "$$ = appendC($1, $2)"),
                            rule({symbolItem("D"), literalItem("d")}, "$$ = makeC($1, $2)"),
                        }),
            symbol("D", {
                            rule({symbolItem("D"), literalItem("e")}, "$$ = appendD($1, $2)"),
                            rule({symbolItem("A"), literalItem("f")}, "$$ = makeD($1, $2)"),
                        }),
        });
    }

    mparse::spec::Specification makeUnresolvedCycleThroughEmptyPrefixSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("E", {
                            emptyRule("$$ = makeE()"),
                        },
                   mparse::spec::SourceReference{.line = 3, .column = 1}),
            symbol("A", {
                            rule({symbolItem("A"), literalItem("a")}, "$$ = appendA($1, $2)"),
                            rule({symbolItem("E"), symbolItem("B"), literalItem("aboba")}, "$$ = makeA($2, $3)"),
                        },
                   mparse::spec::SourceReference{.line = 7, .column = 5}),
            symbol("B", {
                            rule({symbolItem("B"), literalItem("b")}, "$$ = appendB($1, $2)"),
                            rule({symbolItem("A"), literalItem("bobaba")}, "$$ = makeB($1, $2)"),
                        },
                   mparse::spec::SourceReference{.line = 13, .column = 2}),
        });
    }

    mparse::spec::Specification makeSelfStartingSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("A", {
                            rule({symbolItem("A")}, "$$ = makeA($1)"),
                        }),
        });
    }

    mparse::spec::Specification makeEmptyEndCycleSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("B", {
                            emptyRule("$$ = makeB()"),
                        }),
            symbol("A", {
                            rule({symbolItem("A"), symbolItem("B")}, "$$ = appendA($1, $2)"),
                        },
                   mparse::spec::SourceReference{.line = 7, .column = 5}),
        });
    }

    mparse::spec::Specification makeEmptyEndCycleAfterResolveSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("C", {
                            emptyRule("$$ = makeC()"),
                        }),
            symbol("A", {
                            rule({symbolItem("B")}, "$$ = makeA($1)"),
                        },
                   mparse::spec::SourceReference{.line = 11, .column = 3}),
            symbol("B", {
                            rule({symbolItem("A"), symbolItem("C")}, "$$ = makeB($1, $2)"),
                        }),
        });
    }

} // namespace

TEST(ResolveEmptyCycles, IgnoresSelfStartingRules) {
    const auto specification = makeSelfStartingSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result = mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    EXPECT_FALSE(result.unresolved_cycle);
}

TEST(FindEmptyEndCycle, FindsNullableTailOfSelfStartingRule) {
    const auto specification = makeEmptyEndCycleSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);
    auto resolved_empty_cycles =
        mparse::analysis::resolveEmptyCycles(std::move(expanded_symbols_storage));

    const auto cycle =
        mparse::analysis::findEmptyEndCycle(resolved_empty_cycles.symbols_storage);

    ASSERT_TRUE(cycle);
    EXPECT_EQ(
        mparse::analysis::formatEmptyEndCycle(*cycle),
        "A at 7:5 -> A at 7:5"
    );
}

TEST(ResolveEmptyCycles, ResolvesCycleByInliningTargetRules) {
    const auto specification = makeResolvableCycleSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result =
        mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    EXPECT_FALSE(result.unresolved_cycle);

    const auto resolved_a = result.symbols_storage.getSymbolByName("A");
    ASSERT_EQ(resolved_a->rules().size(), 2);

    const auto& self_starting_rule = resolved_a->rules()[0];
    ASSERT_EQ(self_starting_rule.items().size(), 3);
    EXPECT_EQ(mparse::tests::asSymbolItem(self_starting_rule.items()[0]).name, "A");
    EXPECT_EQ(asLiteralItem(self_starting_rule.items()[1]).value, "bobaba");
    EXPECT_EQ(asLiteralItem(self_starting_rule.items()[2]).value, "aboba");

    const auto self_starting_action = self_starting_rule.action();
    ASSERT_TRUE(self_starting_action);
    ASSERT_EQ(self_starting_action->edges().size(), 1);
    EXPECT_EQ(self_starting_action->edges()[0].offset_left, 0);
    EXPECT_EQ(self_starting_action->edges()[0].offset_right, 2);
    ASSERT_TRUE(self_starting_action->edges()[0].node->action().has_value());
    EXPECT_EQ(*self_starting_action->edges()[0].node->action(), "$$ = makeB($1, $2)");

    const auto& terminal_rule = resolved_a->rules()[1];
    ASSERT_EQ(terminal_rule.items().size(), 2);
    EXPECT_EQ(asLiteralItem(terminal_rule.items()[0]).value, "something");
    EXPECT_EQ(asLiteralItem(terminal_rule.items()[1]).value, "aboba");

    const auto terminal_action = terminal_rule.action();
    ASSERT_TRUE(terminal_action);
    ASSERT_EQ(terminal_action->edges().size(), 1);
    EXPECT_EQ(terminal_action->edges()[0].offset_left, 0);
    EXPECT_EQ(terminal_action->edges()[0].offset_right, 1);
    ASSERT_TRUE(terminal_action->edges()[0].node->action().has_value());
    EXPECT_EQ(*terminal_action->edges()[0].node->action(), "$$ = makeSomething($1)");
}

TEST(ResolveEmptyCycles, UpdatesNestedActionEdgesAfterRepeatedInlining) {
    const auto specification = makeResolvableThreeSymbolCycleSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result =
        mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    EXPECT_FALSE(result.unresolved_cycle);

    const auto resolved_a = result.symbols_storage.getSymbolByName("A");
    ASSERT_EQ(resolved_a->rules().size(), 2);

    const auto& self_starting_rule = resolved_a->rules()[0];
    ASSERT_EQ(self_starting_rule.items().size(), 4);
    EXPECT_EQ(mparse::tests::asSymbolItem(self_starting_rule.items()[0]).name, "A");
    EXPECT_EQ(asLiteralItem(self_starting_rule.items()[1]).value, "c");
    EXPECT_EQ(asLiteralItem(self_starting_rule.items()[2]).value, "b");
    EXPECT_EQ(asLiteralItem(self_starting_rule.items()[3]).value, "a");

    const auto a_action = self_starting_rule.action();
    ASSERT_TRUE(a_action);
    ASSERT_EQ(a_action->edges().size(), 1);
    EXPECT_EQ(a_action->edges()[0].offset_left, 0);
    EXPECT_EQ(a_action->edges()[0].offset_right, 3);

    const auto b_action = a_action->edges()[0].node;
    ASSERT_TRUE(b_action);
    ASSERT_TRUE(b_action->action().has_value());
    EXPECT_EQ(*b_action->action(), "$$ = makeB($1, $2)");
    ASSERT_EQ(b_action->edges().size(), 1);
    EXPECT_EQ(b_action->edges()[0].offset_left, 0);
    EXPECT_EQ(b_action->edges()[0].offset_right, 2);

    const auto c_action = b_action->edges()[0].node;
    ASSERT_TRUE(c_action);
    ASSERT_TRUE(c_action->action().has_value());
    EXPECT_EQ(*c_action->action(), "$$ = makeC($1, $2)");
}

TEST(ResolveEmptyCycles, ResolvesComponentWithOneSelfLoop) {
    const auto specification = makeResolvableCycleWithOneSelfLoopSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result = mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    EXPECT_FALSE(result.unresolved_cycle);

    const auto resolved_a = result.symbols_storage.getSymbolByName("A");
    ASSERT_EQ(resolved_a->rules().size(), 3);

    const auto& original_self_loop = resolved_a->rules()[0];
    ASSERT_EQ(original_self_loop.items().size(), 2);
    EXPECT_EQ(mparse::tests::asSymbolItem(original_self_loop.items()[0]).name, "A");
    EXPECT_EQ(asLiteralItem(original_self_loop.items()[1]).value, "self");

    const auto& inlined_self_loop = resolved_a->rules()[1];
    ASSERT_EQ(inlined_self_loop.items().size(), 3);
    EXPECT_EQ(mparse::tests::asSymbolItem(inlined_self_loop.items()[0]).name, "A");
    EXPECT_EQ(asLiteralItem(inlined_self_loop.items()[1]).value, "b");
    EXPECT_EQ(asLiteralItem(inlined_self_loop.items()[2]).value, "a");

    const auto& terminal_rule = resolved_a->rules()[2];
    ASSERT_EQ(terminal_rule.items().size(), 2);
    EXPECT_EQ(asLiteralItem(terminal_rule.items()[0]).value, "x");
    EXPECT_EQ(asLiteralItem(terminal_rule.items()[1]).value, "a");
}

TEST(ResolveEmptyCycles, RestoresCycleThroughInlinedEdges) {
    const auto specification = makeUnresolvedCycleThroughInlinedEdgesSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result = mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    ASSERT_TRUE(result.unresolved_cycle);
    EXPECT_EQ(
        mparse::analysis::formatEmptyCycle(*result.unresolved_cycle),
        "C at 1:1 -> D at 1:1 -> A at 1:1 -> B at 1:1 -> C at 1:1"
    );
}

TEST(ResolveEmptyCycles, RestoresCycleThroughEmptyPrefixExpansion) {
    const auto specification = makeUnresolvedCycleThroughEmptyPrefixSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result = mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    ASSERT_TRUE(result.unresolved_cycle);
    EXPECT_EQ(
        mparse::analysis::formatEmptyCycle(*result.unresolved_cycle),
        "A at 7:5 -> E at 3:1 -> B at 13:2 -> A at 7:5"
    );
}

TEST(ResolveEmptyCycles, KeepsUnresolvedCycleWithTwoSelfLoops) {
    const auto specification = makeUnresolvedCycleSpecification();
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto result = mparse::analysis::resolveEmptyCycles(expanded_symbols_storage);

    ASSERT_TRUE(result.unresolved_cycle);
    EXPECT_EQ(
        mparse::analysis::formatEmptyCycle(*result.unresolved_cycle),
        "A at 10:2 -> B at 20:3 -> A at 10:2"
    );
}

TEST(ResolveEmptyCycles, RestoresCycleThroughOriginalSymbols) {
    const auto result =
        mparse::analysis::resolveEmptyCycles(makeCycleThroughSyntheticSymbolStorage());

    ASSERT_TRUE(result.unresolved_cycle);
    EXPECT_EQ(
        mparse::analysis::formatEmptyCycle(*result.unresolved_cycle),
        "A at 30:4 -> B at 40:5 -> A at 30:4"
    );
}

TEST(Analyze, AllowsResolvableCycleByDefault) {
    const auto specification = makeResolvableCycleSpecification();

    EXPECT_NO_THROW((void) mparse::analysis::analyzeOrThrow(specification));
}

TEST(Analyze, AllowsResolvableCycleWithOneSelfLoopByDefault) {
    const auto specification = makeResolvableCycleWithOneSelfLoopSpecification();

    EXPECT_NO_THROW((void) mparse::analysis::analyzeOrThrow(specification));
}

TEST(Analyze, ThrowsOnUnresolvedNonProgressingCycleByDefault) {
    const auto specification = makeUnresolvedCycleSpecification();

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected non-progressing cycle error";
    } catch (const mparse::analysis::AnalysisError& error) {

        EXPECT_EQ(error.kind(), mparse::analysis::AnalysisErrorKind::NonProgressingCycle);
        expectMatchesCanon(
            "analysis/canons/errors/non_progressing_cycle.txt",
            error.what()
        );

        ASSERT_EQ(error.sourceReferences().size(), 3);
        EXPECT_EQ(error.sourceReferences()[0].name, "A");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 10);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 2);
        EXPECT_EQ(error.sourceReferences()[1].name, "B");
        EXPECT_EQ(error.sourceReferences()[1].source_reference.line, 20);
        EXPECT_EQ(error.sourceReferences()[1].source_reference.column, 3);
        EXPECT_EQ(error.sourceReferences()[2].name, "A");
        EXPECT_EQ(error.sourceReferences()[2].source_reference.line, 10);
        EXPECT_EQ(error.sourceReferences()[2].source_reference.column, 2);
    }
}

TEST(Analyze, ThrowsOnUnresolvedNonProgressingCycleThroughEmptyPrefix) {
    const auto specification = makeUnresolvedCycleThroughEmptyPrefixSpecification();

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected non-progressing cycle error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(error.kind(), mparse::analysis::AnalysisErrorKind::NonProgressingCycle);

        expectMatchesCanon(
            "analysis/canons/errors/non_progressing_cycle_through_empty_prefix.txt",
            error.what()
        );

        ASSERT_EQ(error.sourceReferences().size(), 4);
        EXPECT_EQ(error.sourceReferences()[0].name, "A");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 5);
        EXPECT_EQ(error.sourceReferences()[1].name, "E");
        EXPECT_EQ(error.sourceReferences()[1].source_reference.line, 3);
        EXPECT_EQ(error.sourceReferences()[1].source_reference.column, 1);
        EXPECT_EQ(error.sourceReferences()[2].name, "B");
        EXPECT_EQ(error.sourceReferences()[2].source_reference.line, 13);
        EXPECT_EQ(error.sourceReferences()[2].source_reference.column, 2);
        EXPECT_EQ(error.sourceReferences()[3].name, "A");
        EXPECT_EQ(error.sourceReferences()[3].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[3].source_reference.column, 5);
    }
}

TEST(Analyze, ThrowsOnEmptyEndCycleByDefault) {
    const auto specification = makeEmptyEndCycleSpecification();

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected empty end-to-end cycle error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(error.kind(), mparse::analysis::AnalysisErrorKind::EmptyEndCycle);
        EXPECT_EQ(
            std::string{error.what()},
            "empty end-to-end automaton cycle found: A at 7:5 -> A at 7:5"
        );

        ASSERT_EQ(error.sourceReferences().size(), 2);
        EXPECT_EQ(error.sourceReferences()[0].name, "A");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 5);
        EXPECT_EQ(error.sourceReferences()[1].name, "A");
        EXPECT_EQ(error.sourceReferences()[1].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[1].source_reference.column, 5);
    }
}

TEST(Analyze, ThrowsOnEmptyEndCycleCreatedAfterResolvingFirstSymbolCycle) {
    const auto specification = makeEmptyEndCycleAfterResolveSpecification();

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected empty end-to-end cycle error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(error.kind(), mparse::analysis::AnalysisErrorKind::EmptyEndCycle);
        EXPECT_EQ(
            std::string{error.what()},
            "empty end-to-end automaton cycle found: A at 11:3 -> A at 11:3"
        );
    }
}

TEST(Analyze, CanDisableNonProgressingCycleCheck) {
    const auto specification = makeResolvableCycleSpecification();

    const auto symbols_storage = mparse::analysis::analyzeOrThrow(
        specification,
        mparse::analysis::AnalysisOptions{
            .check_nonprogressing_cycles = false,
        }
    );

    const auto symbol_a = symbols_storage.getSymbolByName("A");
    ASSERT_EQ(symbol_a->rules().size(), 1);
    ASSERT_EQ(symbol_a->rules()[0].items().size(), 2);
    EXPECT_EQ(mparse::tests::asSymbolItem(symbol_a->rules()[0].items()[0]).name, "B");
}
