#include <analysis/algo/expand_empty_items.h>
#include <analysis/error.h>
#include <analysis/rule.h>
#include <analysis/symbols_storage.h>
#include <analysis/utils/rules.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace {

    bool containsEmptySymbolReference(
        const mparse::analysis::ActionTreeNodePtr& action,
        std::unordered_set<const mparse::analysis::ActionTreeNode*>& visited
    ) {
        if (!action || !visited.insert(action.get()).second) {
            return false;
        }

        if (action->kind() == mparse::analysis::ActionTreeNodeKind::EmptySymbolReference) {
            return true;
        }

        return std::ranges::any_of(
            action->edges(),
            [&](const mparse::analysis::ActionTreeEdge& edge) {
                return containsEmptySymbolReference(edge.node, visited);
            }
        );
    }

    bool containsEmptySymbolReference(
        const mparse::analysis::ActionTreeNodePtr& action
    ) {
        std::unordered_set<const mparse::analysis::ActionTreeNode*> visited;
        return containsEmptySymbolReference(action, visited);
    }

} // namespace

TEST(ExpandEmptyItems, CollapsedNullablePrefixesKeepEmptyActionsInOrder) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("A", {
                        emptyRule("$$ = makeA()"),
                    }),
        symbol("B", {
                        emptyRule("$$ = makeB()"),
                    }),
        symbol("C", {
                        rule({literalItem("c")}, "$$ = $1"),
                    }),
        symbol("S", {
                        rule({symbolItem("A"), symbolItem("B"), symbolItem("C")}, "$$ = makeS($1, $2, $3)"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto expanded_s = expanded_symbols_storage.getSymbolByName("S");
    ASSERT_EQ(expanded_s->rules().size(), 5);

    const auto& collapsed_rule = expanded_s->rules()[2];
    ASSERT_EQ(collapsed_rule.items().size(), 1);
    EXPECT_EQ(asSymbolItem(collapsed_rule.items()[0]).name, "C");

    const auto action = collapsed_rule.action();
    ASSERT_TRUE(action);
    EXPECT_EQ(action->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    EXPECT_EQ(action->size(), 1);
    ASSERT_TRUE(action->action().has_value());
    EXPECT_EQ(*action->action(), "$$ = makeS($1, $2, $3)");
    ASSERT_EQ(action->edges().size(), 2);

    EXPECT_EQ(action->edges()[0].offset_left, 0);
    EXPECT_EQ(action->edges()[0].offset_right, 0);
    EXPECT_EQ(action->edges()[0].node->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(action->edges()[0].node->action().has_value());
    EXPECT_EQ(*action->edges()[0].node->action(), "$$ = makeA()");

    EXPECT_EQ(action->edges()[1].offset_left, 0);
    EXPECT_EQ(action->edges()[1].offset_right, 0);
    EXPECT_EQ(action->edges()[1].node->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(action->edges()[1].node->action().has_value());
    EXPECT_EQ(*action->edges()[1].node->action(), "$$ = makeB()");

    const auto expanded_a = expanded_symbols_storage.getSymbolByName("A");
    ASSERT_EQ(expanded_a->rules().size(), 3);
    EXPECT_EQ(
        expanded_a->rules()[0].action()->kind(),
        mparse::analysis::ActionTreeNodeKind::ForwardSemanticValue
    );
    EXPECT_EQ(
        expanded_a->rules()[2].action()->kind(),
        mparse::analysis::ActionTreeNodeKind::ForwardSemanticValue
    );

    EXPECT_FALSE(containsEmptySymbolReference(action));
}

TEST(ExpandEmptyItems, UsesDedicatedActionKindsForSyntheticRules) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("A", {
                        rule({literalItem("a")}, "$$ = $1"),
                        emptyRule("$$ = makeEmptyA()"),
                        rule({literalItem("b")}, "$$ = $1"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto expanded_a = expanded_symbols_storage.getSymbolByName("A");
    ASSERT_EQ(expanded_a->rules().size(), 3);

    const auto before_action = expanded_a->rules()[0].action();
    ASSERT_TRUE(before_action);
    EXPECT_EQ(before_action->kind(), mparse::analysis::ActionTreeNodeKind::ForwardSemanticValue);
    EXPECT_FALSE(before_action->action().has_value());
    EXPECT_FALSE(before_action->symbolName().has_value());
    EXPECT_EQ(before_action->size(), 1);

    const auto empty_action = expanded_a->rules()[1].action();
    ASSERT_TRUE(empty_action);
    EXPECT_EQ(empty_action->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(empty_action->action().has_value());
    EXPECT_EQ(*empty_action->action(), "$$ = makeEmptyA()");

    const auto after_action = expanded_a->rules()[2].action();
    ASSERT_TRUE(after_action);
    EXPECT_EQ(after_action->kind(), mparse::analysis::ActionTreeNodeKind::ForwardSemanticValue);
    EXPECT_FALSE(after_action->action().has_value());
    EXPECT_FALSE(after_action->symbolName().has_value());
    EXPECT_EQ(after_action->size(), 1);
}

TEST(ExpandEmptyItems, KeepsSelfStartingCollapsedRulesOnOriginalSymbol) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("B", {
                        emptyRule("$$ = makeB()"),
                    }),
        symbol("C", {
                        rule({literalItem("c")}, "$$ = $1"),
                    }),
        symbol("A", {
                        emptyRule("$$ = makeEmptyA()"),
                        rule({symbolItem("B"), symbolItem("A"), symbolItem("C")}, "$$ = makeA($1, $2, $3)"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto expanded_a_before = expanded_symbols_storage.getSymbolByName("ABeforeEmptyPart");
    EXPECT_TRUE(expanded_a_before->rules().empty());
    EXPECT_TRUE(std::ranges::none_of(expanded_a_before->rules(), [](const mparse::analysis::Rule& rule) {
        return mparse::analysis::startsWithSymbol(rule, "A");
    }));

    const auto expanded_a_after = expanded_symbols_storage.getSymbolByName("AAfterEmptyPart");
    ASSERT_EQ(expanded_a_after->rules().size(), 2);
    EXPECT_TRUE(std::ranges::none_of(expanded_a_after->rules(), [](const mparse::analysis::Rule& rule) {
        return mparse::analysis::startsWithSymbol(rule, "A");
    }));

    const auto expanded_a = expanded_symbols_storage.getSymbolByName("A");
    ASSERT_EQ(expanded_a->rules().size(), 4);

    const auto& self_starting_rule = expanded_a->rules().back();
    ASSERT_EQ(self_starting_rule.items().size(), 2);
    EXPECT_EQ(asSymbolItem(self_starting_rule.items()[0]).name, "A");
    EXPECT_EQ(asSymbolItem(self_starting_rule.items()[1]).name, "C");
}

TEST(ExpandEmptyItems, ExpandsNullableTailOfSelfStartingRules) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("B", {
                        rule({literalItem("b")}, "$$ = makeBeforeB($1)"),
                        emptyRule("$$ = makeEmptyB()"),
                        rule({literalItem("d")}, "$$ = makeAfterB($1)"),
                    }),
        symbol("C", {
                        rule({literalItem("c")}, "$$ = makeC($1)"),
                    }),
        symbol("A", {
                        rule({symbolItem("A"), symbolItem("B"), symbolItem("C")}, "$$ = makeA($1, $2, $3)"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto expanded_a = expanded_symbols_storage.getSymbolByName("A");
    ASSERT_EQ(expanded_a->rules().size(), 3);

    const auto& before_empty_rule = expanded_a->rules()[0];
    ASSERT_EQ(before_empty_rule.items().size(), 3);
    EXPECT_EQ(asSymbolItem(before_empty_rule.items()[0]).name, "A");
    EXPECT_EQ(asSymbolItem(before_empty_rule.items()[1]).name, "BBeforeEmptyPart");
    EXPECT_EQ(asSymbolItem(before_empty_rule.items()[2]).name, "C");

    const auto& collapsed_rule = expanded_a->rules()[1];
    ASSERT_EQ(collapsed_rule.items().size(), 2);
    EXPECT_EQ(asSymbolItem(collapsed_rule.items()[0]).name, "A");
    EXPECT_EQ(asSymbolItem(collapsed_rule.items()[1]).name, "C");

    const auto collapsed_action = collapsed_rule.action();
    ASSERT_TRUE(collapsed_action);
    EXPECT_EQ(collapsed_action->size(), 2);
    ASSERT_EQ(collapsed_action->edges().size(), 1);

    const auto& empty_b_edge = collapsed_action->edges()[0];
    EXPECT_EQ(empty_b_edge.offset_left, 1);
    EXPECT_EQ(empty_b_edge.offset_right, 1);
    ASSERT_TRUE(empty_b_edge.node);
    EXPECT_EQ(empty_b_edge.node->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(empty_b_edge.node->action().has_value());
    EXPECT_EQ(*empty_b_edge.node->action(), "$$ = makeEmptyB()");

    const auto& after_empty_rule = expanded_a->rules()[2];
    ASSERT_EQ(after_empty_rule.items().size(), 3);
    EXPECT_EQ(asSymbolItem(after_empty_rule.items()[0]).name, "A");
    EXPECT_EQ(asSymbolItem(after_empty_rule.items()[1]).name, "BAfterEmptyPart");
    EXPECT_EQ(asSymbolItem(after_empty_rule.items()[2]).name, "C");
}

TEST(ExpandEmptyItems, RejectsSelfStartingRuleWithSelfTail) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("A", {
                        rule({symbolItem("A"), symbolItem("A")}, "$$ = merge($1, $2)"),
                    },
               mparse::spec::SourceReference{.line = 7, .column = 5}),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    try {
        (void) mparse::analysis::expandEmptyItems(symbols_storage);
        FAIL() << "expected unsupported self-starting rule error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(
            error.kind(),
            mparse::analysis::AnalysisErrorKind::UnsupportedSelfStartingRule
        );
        EXPECT_EQ(
            std::string{error.what()},
            "unsupported self-starting rule found: A at 7:5 starts with two "
            "references to A; rules like A : A A are not supported yet"
        );

        ASSERT_EQ(error.sourceReferences().size(), 1);
        EXPECT_EQ(error.sourceReferences()[0].name, "A");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 5);
    }
}

TEST(ExpandEmptyItems, ResolvesNullableCyclesIntoActionGraphCycles) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("A", {
                        rule({symbolItem("B")}, "$$ = makeA($1)"),
                    }),
        symbol("B", {
                        rule({symbolItem("A")}, "$$ = makeB($1)"),
                        emptyRule("$$ = makeEmptyB()"),
                    }),
    });

    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(specification);

    const auto expanded_symbols_storage =
        mparse::analysis::expandEmptyItems(symbols_storage);

    const auto expanded_a = expanded_symbols_storage.getSymbolByName("A");
    ASSERT_EQ(expanded_a->rules().size(), 3);

    const auto empty_a_action = expanded_a->rules()[1].action();
    ASSERT_TRUE(empty_a_action);
    EXPECT_EQ(empty_a_action->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_EQ(empty_a_action->edges().size(), 1);
    const auto empty_b_from_a = empty_a_action->edges()[0].node;
    ASSERT_TRUE(empty_b_from_a);
    EXPECT_EQ(empty_b_from_a->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(empty_b_from_a->action().has_value());
    EXPECT_EQ(*empty_b_from_a->action(), "$$ = makeB($1)");

    const auto expanded_b = expanded_symbols_storage.getSymbolByName("B");
    ASSERT_EQ(expanded_b->rules().size(), 3);

    const auto empty_b_action = expanded_b->rules()[1].action();
    ASSERT_TRUE(empty_b_action);
    EXPECT_EQ(empty_b_action->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_EQ(empty_b_action->edges().size(), 1);
    const auto empty_a_from_b = empty_b_action->edges()[0].node;
    ASSERT_TRUE(empty_a_from_b);
    EXPECT_EQ(empty_a_from_b->kind(), mparse::analysis::ActionTreeNodeKind::RuleAction);
    ASSERT_TRUE(empty_a_from_b->action().has_value());
    EXPECT_EQ(*empty_a_from_b->action(), "$$ = makeA($1)");

    EXPECT_EQ(empty_b_from_a, empty_b_action);
    EXPECT_EQ(empty_a_from_b, empty_a_action);

    EXPECT_FALSE(containsEmptySymbolReference(empty_a_action));
    EXPECT_FALSE(containsEmptySymbolReference(empty_b_action));
}
