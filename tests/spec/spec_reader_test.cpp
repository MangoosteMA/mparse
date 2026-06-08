#include <spec/spec.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

    std::filesystem::path sourcePath(const std::string& file_name) {
        return std::filesystem::path{kMparseTestSourcesDir} / file_name;
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream file{path};
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }

} // namespace

TEST(SpecReader, ParsesSymbolsRulesLiteralsRangesAndActions) {
    const auto specification = mparse::spec::readSpecificationOrThrow(
        sourcePath("valid_spec.grammar")
    );

    ASSERT_EQ(specification.symbols.size(), 3);

    const auto& expression = specification.symbols[0];
    EXPECT_EQ(expression.name, "Expression");
    EXPECT_EQ(expression.source_reference.line, 5);
    EXPECT_EQ(expression.source_reference.column, 1);
    EXPECT_EQ(expression.formatWithSourceReference(), "Expression at 5:1");
    ASSERT_TRUE(expression.type.has_value());
    EXPECT_EQ(*expression.type, "int");
    ASSERT_EQ(expression.rules.size(), 2);
    ASSERT_EQ(expression.rules[1].items.size(), 3);

    const auto* plus =
        std::get_if<mparse::spec::RuleItemLiteral>(&expression.rules[1].items[1].value);
    ASSERT_NE(plus, nullptr);
    EXPECT_EQ(plus->value, "+");
    EXPECT_FALSE(plus->empty());

    const mparse::spec::Symbol& integer = specification.symbols[1];
    EXPECT_EQ(integer.source_reference.line, 9);
    EXPECT_EQ(integer.source_reference.column, 1);
    ASSERT_EQ(integer.rules.size(), 2);

    const auto* digit_range =
        std::get_if<mparse::spec::RuleItemRange>(&integer.rules[0].items[0].value);
    ASSERT_NE(digit_range, nullptr);
    EXPECT_EQ(digit_range->from, '0');
    EXPECT_EQ(digit_range->to, '9');
    EXPECT_FALSE(digit_range->empty());
    ASSERT_TRUE(integer.rules[0].action.has_value());
    EXPECT_EQ(*integer.rules[0].action, "$$ = $1");

    const auto* text_literal =
        std::get_if<mparse::spec::RuleItemLiteral>(&integer.rules[1].items[0].value);
    ASSERT_NE(text_literal, nullptr);
    EXPECT_EQ(text_literal->value, "text");
    EXPECT_FALSE(text_literal->empty());

    const mparse::spec::Symbol& parameterized = specification.symbols[2];
    EXPECT_EQ(parameterized.source_reference.line, 13);
    EXPECT_EQ(parameterized.source_reference.column, 1);
    ASSERT_EQ(parameterized.arguments.size(), 1);
    EXPECT_EQ(parameterized.arguments[0].name, "offset");
    EXPECT_EQ(parameterized.arguments[0].type, "int");

    const auto* symbol_reference =
        std::get_if<mparse::spec::RuleItemSymbol>(&parameterized.rules[0].items[0].value);
    ASSERT_NE(symbol_reference, nullptr);
    EXPECT_EQ(symbol_reference->name, "Parameterized");
    ASSERT_EQ(symbol_reference->arguments.size(), 1);
    EXPECT_EQ(symbol_reference->arguments[0], "offset + 1");
}

TEST(SpecReader, DetectsEmptyLiteralAndRangeItems) {
    EXPECT_TRUE(mparse::spec::RuleItemLiteral{.value = ""}.empty());
    EXPECT_FALSE(mparse::spec::RuleItemLiteral{.value = "x"}.empty());

    EXPECT_TRUE((mparse::spec::RuleItemRange{.from = 'z', .to = 'a'}).empty());
    EXPECT_FALSE((mparse::spec::RuleItemRange{.from = 'a', .to = 'z'}).empty());
    EXPECT_FALSE((mparse::spec::RuleItemRange{.from = 'a', .to = 'a'}).empty());
}

TEST(SpecReader, SourceTemplateInsertsContentBetweenGrammarMarkers) {
    const auto specification =
        mparse::spec::readSpecificationOrThrow(sourcePath("source_template.grammar"));

    EXPECT_EQ(specification.source_template.insert("generated\n"), readFile(sourcePath("source_template_expected.txt")));
}

TEST(SpecReader, ThrowsOnUnsupportedGroupedExpressions) {
    EXPECT_THROW(
        {
            mparse::spec::readSpecificationOrThrow(sourcePath(
                "unsupported_grouped_expression.grammar"
            ));
        },
        std::runtime_error
    );
}
