#include <spec/spec.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

    mparse::spec::RegexExpressionPtr regexExpression(
        mparse::spec::RegexExpressionValue value
    ) {
        return std::make_shared<mparse::spec::RegexExpression>(
            mparse::spec::RegexExpression{.value = std::move(value)}
        );
    }

} // namespace

TEST(SpecReader, ParsesSymbolsRulesLiteralsRangesAndActions) {
    const auto specification = mparse::spec::readSpecificationOrThrow(
        sourcePath("valid_spec.grammar")
    );

    ASSERT_EQ(specification.symbols.size(), 4);

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

    const mparse::spec::Symbol& indent = specification.symbols[3];
    EXPECT_EQ(indent.source_reference.line, 16);
    EXPECT_EQ(indent.source_reference.column, 1);
    ASSERT_EQ(indent.rules.size(), 2);

    const auto* repeated_literal =
        std::get_if<mparse::spec::RuleItemRepeatedLiteral>(&indent.rules[0].items[0].value);
    ASSERT_NE(repeated_literal, nullptr);
    EXPECT_EQ(repeated_literal->value, " ");
    EXPECT_EQ(repeated_literal->count_expression, "offset");
    EXPECT_FALSE(repeated_literal->empty());

    const auto* parenthesized_repeated_literal =
        std::get_if<mparse::spec::RuleItemRepeatedLiteral>(&indent.rules[1].items[0].value);
    ASSERT_NE(parenthesized_repeated_literal, nullptr);
    EXPECT_EQ(parenthesized_repeated_literal->value, " ");
    EXPECT_EQ(parenthesized_repeated_literal->count_expression, "offset + 1");
}

TEST(SpecReader, DetectsEmptyLiteralAndRangeItems) {
    EXPECT_TRUE(mparse::spec::RuleItemLiteral{.value = ""}.empty());
    EXPECT_FALSE(mparse::spec::RuleItemLiteral{.value = "x"}.empty());
    EXPECT_TRUE((mparse::spec::RuleItemRepeatedLiteral{.value = "", .count_expression = "n"}).empty());
    EXPECT_FALSE((mparse::spec::RuleItemRepeatedLiteral{.value = "x", .count_expression = "n"}).empty());

    EXPECT_TRUE((mparse::spec::RuleItemRange{.from = 'z', .to = 'a'}).empty());
    EXPECT_FALSE((mparse::spec::RuleItemRange{.from = 'a', .to = 'z'}).empty());
    EXPECT_FALSE((mparse::spec::RuleItemRange{.from = 'a', .to = 'a'}).empty());
}

TEST(SpecReader, DetectsEmptyRegexAstItems) {
    const auto empty_literal =
        regexExpression(mparse::spec::RegexLiteral{.value = ""});
    const auto literal =
        regexExpression(mparse::spec::RegexLiteral{.value = "x"});

    EXPECT_TRUE(empty_literal->empty());
    EXPECT_FALSE(literal->empty());

    EXPECT_TRUE((mparse::spec::RegexRepeat{
        .item = literal,
        .kind = mparse::spec::RegexRepeatKind::ZeroOrMore,
    }).empty());
    EXPECT_FALSE((mparse::spec::RegexRepeat{
        .item = literal,
        .kind = mparse::spec::RegexRepeatKind::OneOrMore,
    }).empty());
    EXPECT_TRUE((mparse::spec::RegexRepeat{
        .item = empty_literal,
        .kind = mparse::spec::RegexRepeatKind::OneOrMore,
    }).empty());

    EXPECT_FALSE((mparse::spec::RegexSequence{
        .items = {empty_literal, literal},
    }).empty());
    EXPECT_TRUE((mparse::spec::RegexAlternative{
        .alternatives = {literal, empty_literal},
    }).empty());
    EXPECT_TRUE((mparse::spec::RuleItemRegex{
        .expression = mparse::spec::RegexExpression{
            .value = mparse::spec::RegexAlternative{
                .alternatives = {literal, empty_literal},
            },
        },
    }).empty());
}

TEST(SpecReader, ParsesEscapedLiteralCharacters) {
    const auto specification = mparse::spec::readSpecificationOrThrow(
        sourcePath("escaped_literals.grammar")
    );

    ASSERT_EQ(specification.symbols.size(), 1);
    const auto& symbol = specification.symbols[0];
    ASSERT_EQ(symbol.rules.size(), 4);

    const auto* newline =
        std::get_if<mparse::spec::RuleItemLiteral>(&symbol.rules[0].items[0].value);
    ASSERT_NE(newline, nullptr);
    EXPECT_EQ(newline->value, "\n");

    const auto* tab_to_newline =
        std::get_if<mparse::spec::RuleItemRange>(&symbol.rules[1].items[0].value);
    ASSERT_NE(tab_to_newline, nullptr);
    EXPECT_EQ(tab_to_newline->from, '\t');
    EXPECT_EQ(tab_to_newline->to, '\n');

    const auto* backslash =
        std::get_if<mparse::spec::RuleItemLiteral>(&symbol.rules[2].items[0].value);
    ASSERT_NE(backslash, nullptr);
    EXPECT_EQ(backslash->value, "\\");

    const auto* quote =
        std::get_if<mparse::spec::RuleItemLiteral>(&symbol.rules[3].items[0].value);
    ASSERT_NE(quote, nullptr);
    EXPECT_EQ(quote->value, "'");
}

TEST(SpecReader, SourceTemplateInsertsContentBetweenGrammarMarkers) {
    const auto specification =
        mparse::spec::readSpecificationOrThrow(sourcePath("source_template.grammar"));

    EXPECT_EQ(specification.source_template.insert("generated\n"), readFile(sourcePath("source_template_expected.txt")));
}

TEST(SpecReader, ParsesRegexExpressions) {
    const auto specification = mparse::spec::readSpecificationOrThrow(
        sourcePath("regex_expression.grammar")
    );

    ASSERT_EQ(specification.symbols.size(), 1);
    const auto& symbol = specification.symbols.front();
    ASSERT_EQ(symbol.rules.size(), 2);
    ASSERT_EQ(symbol.rules[0].items.size(), 2);

    const auto* head =
        std::get_if<mparse::spec::RuleItemRegex>(&symbol.rules[0].items[0].value);
    ASSERT_NE(head, nullptr);
    EXPECT_TRUE(std::holds_alternative<mparse::spec::RegexAlternative>(
        head->expression.value
    ));

    const auto* tail =
        std::get_if<mparse::spec::RuleItemRegex>(&symbol.rules[0].items[1].value);
    ASSERT_NE(tail, nullptr);
    const auto* tail_repeat =
        std::get_if<mparse::spec::RegexRepeat>(&tail->expression.value);
    ASSERT_NE(tail_repeat, nullptr);
    EXPECT_EQ(tail_repeat->kind, mparse::spec::RegexRepeatKind::ZeroOrMore);

    ASSERT_EQ(symbol.rules[1].items.size(), 1);
    const auto* number =
        std::get_if<mparse::spec::RuleItemRegex>(&symbol.rules[1].items[0].value);
    ASSERT_NE(number, nullptr);
    const auto* number_repeat =
        std::get_if<mparse::spec::RegexRepeat>(&number->expression.value);
    ASSERT_NE(number_repeat, nullptr);
    EXPECT_EQ(number_repeat->kind, mparse::spec::RegexRepeatKind::OneOrMore);
}
