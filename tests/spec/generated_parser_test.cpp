#include <spec/generated/generated_parser.h>
#include <spec/spec.h>
#include <spec/utils.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

    std::filesystem::path sourcePath(const std::string& file_name) {
        return std::filesystem::path{kMparseTestSourcesDir} / file_name;
    }

    std::filesystem::path projectPath(const std::string& file_name) {
        return std::filesystem::path{kMparseProjectSourceDir} / file_name;
    }

    void shiftLines(std::vector<mparse::spec::Symbol>& symbols, size_t first_line) {
        for (auto& symbol : symbols) {
            symbol.source_reference.line += first_line - 1;
        }
    }

    void expectArgumentsEqual(
        const std::vector<mparse::spec::SymbolArgument>& actual,
        const std::vector<mparse::spec::SymbolArgument>& expected
    ) {
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t index = 0; index < actual.size(); index++) {
            EXPECT_EQ(actual[index].name, expected[index].name);
            EXPECT_EQ(actual[index].type, expected[index].type);
        }
    }

    void expectRuleItemValuesEqual(
        const mparse::spec::RuleItemValue& actual,
        const mparse::spec::RuleItemValue& expected
    ) {
        ASSERT_EQ(actual.index(), expected.index());

        if (const auto* actual_literal =
                std::get_if<mparse::spec::RuleItemLiteral>(&actual)) {
            const auto& expected_literal =
                std::get<mparse::spec::RuleItemLiteral>(expected);
            EXPECT_EQ(actual_literal->value, expected_literal.value);
            return;
        }

        if (const auto* actual_range =
                std::get_if<mparse::spec::RuleItemRange>(&actual)) {
            const auto& expected_range = std::get<mparse::spec::RuleItemRange>(expected);
            EXPECT_EQ(actual_range->from, expected_range.from);
            EXPECT_EQ(actual_range->to, expected_range.to);
            return;
        }

        if (const auto* actual_repeated =
                std::get_if<mparse::spec::RuleItemRepeatedLiteral>(&actual)) {
            const auto& expected_repeated =
                std::get<mparse::spec::RuleItemRepeatedLiteral>(expected);
            EXPECT_EQ(actual_repeated->value, expected_repeated.value);
            EXPECT_EQ(
                actual_repeated->count_expression,
                expected_repeated.count_expression
            );
            return;
        }

        const auto& actual_symbol = std::get<mparse::spec::RuleItemSymbol>(actual);
        const auto& expected_symbol = std::get<mparse::spec::RuleItemSymbol>(expected);
        EXPECT_EQ(actual_symbol.name, expected_symbol.name);
        EXPECT_EQ(actual_symbol.arguments, expected_symbol.arguments);
    }

    void expectRulesEqual(
        const std::vector<mparse::spec::Rule>& actual,
        const std::vector<mparse::spec::Rule>& expected
    ) {
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t rule_index = 0; rule_index < actual.size(); rule_index++) {
            SCOPED_TRACE("rule " + std::to_string(rule_index));
            EXPECT_EQ(actual[rule_index].action, expected[rule_index].action);
            ASSERT_EQ(actual[rule_index].items.size(), expected[rule_index].items.size());

            for (size_t item_index = 0; item_index < actual[rule_index].items.size(); item_index++) {
                SCOPED_TRACE("item " + std::to_string(item_index));
                expectRuleItemValuesEqual(
                    actual[rule_index].items[item_index].value,
                    expected[rule_index].items[item_index].value
                );
            }
        }
    }

    void expectSymbolsEqual(
        const std::vector<mparse::spec::Symbol>& actual,
        const std::vector<mparse::spec::Symbol>& expected
    ) {
        ASSERT_EQ(actual.size(), expected.size());

        for (size_t symbol_index = 0; symbol_index < actual.size(); symbol_index++) {
            SCOPED_TRACE("symbol " + std::to_string(symbol_index));

            EXPECT_EQ(actual[symbol_index].name, expected[symbol_index].name);
            EXPECT_EQ(actual[symbol_index].source_reference.line, expected[symbol_index].source_reference.line);
            EXPECT_EQ(actual[symbol_index].source_reference.column, expected[symbol_index].source_reference.column);
            EXPECT_EQ(actual[symbol_index].type, expected[symbol_index].type);
            expectArgumentsEqual(actual[symbol_index].arguments, expected[symbol_index].arguments);
            expectRulesEqual(actual[symbol_index].rules, expected[symbol_index].rules);
        }
    }

    void expectGeneratedParserMatchesHandParser(const std::filesystem::path& path) {
        const auto extracted = mparse::spec::extractGrammarBlock(
            mparse::spec::readFile(path)
        );

        auto generated_symbols = mparse::spec::generated::parseSymbols(extracted.grammar);
        ASSERT_TRUE(generated_symbols.has_value());
        shiftLines(*generated_symbols, extracted.first_line);

        const auto hand_parsed = mparse::spec::readSpecificationOrThrow(path);
        expectSymbolsEqual(*generated_symbols, hand_parsed.symbols);
    }

} // namespace

TEST(GeneratedSpecParser, MatchesHandParserOnSpecFixtures) {
    expectGeneratedParserMatchesHandParser(sourcePath("valid_spec.grammar"));
    expectGeneratedParserMatchesHandParser(sourcePath("escaped_literals.grammar"));
    expectGeneratedParserMatchesHandParser(sourcePath("source_template.grammar"));
}

TEST(GeneratedSpecParser, MatchesHandParserOnExamples) {
    expectGeneratedParserMatchesHandParser(projectPath("examples/cpp/hex_color.cpp"));
    expectGeneratedParserMatchesHandParser(projectPath("examples/cpp/nested_actions.cpp"));
    expectGeneratedParserMatchesHandParser(projectPath("examples/cpp/parameterized_outline.cpp"));
}
