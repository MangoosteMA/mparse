#include <analysis/analyze.h>
#include <codegen/codegen.h>
#include <utils/canon.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

    std::string generateCpp(const mparse::spec::Specification& specification) {
        const auto symbols_storage = mparse::analysis::analyzeOrThrow(specification);
        const auto automata = symbols_storage.buildAutomata();

        return mparse::codegen::generate(
            specification,
            symbols_storage,
            automata,
            mparse::codegen::GenerationOptions{
                .language = mparse::codegen::Language::Cpp,
                .root_symbol_name = specification.symbols.front().name,
            }
        );
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

    mparse::spec::RegexExpressionPtr regexExpression(
        mparse::spec::RegexExpressionValue value
    ) {
        return std::make_shared<mparse::spec::RegexExpression>(
            mparse::spec::RegexExpression{.value = std::move(value)}
        );
    }

} // namespace

TEST(CodegenCpp, GeneratesLiteralRootParser) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "Root",
                .type = "std::string",
                .rules = {
                    mparse::tests::rule(
                        {mparse::tests::literalItem("x")},
                        "$$ = $1"
                    ),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);
    expectMatchesCanon("codegen/canons/cpp/literal_root_parser.cpp", generated);
}

TEST(CodegenCpp, EmitsRangeAndSymbolTransitions) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "Digit",
                .type = "char",
                .rules = {
                    mparse::tests::rule(
                        {mparse::tests::rangeItem('0', '9')},
                        "$$ = $1"
                    ),
                },
            },
            mparse::spec::Symbol{
                .name = "Root",
                .type = "char",
                .rules = {
                    mparse::tests::rule({mparse::tests::symbolItem("Digit")}, "$$ = $1"),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);
    expectMatchesCanon("codegen/canons/cpp/range_and_symbol_parser.cpp", generated);
}

TEST(CodegenCpp, EmitsOnlySinglePublicParseInterface) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "Root",
                .type = "int",
                .arguments = {
                    mparse::spec::SymbolArgument{
                        .name = "offset",
                        .type = "int",
                    },
                },
                .rules = {
                    mparse::tests::rule({mparse::tests::literalItem("x")}, "$$ = offset"),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);

    EXPECT_NE(
        generated.find("std::optional<int> parse(std::string_view input, int offset)"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("static std::any mparse_action_0(const std::vector<std::any>& args, int offset)"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("std::vector<std::any> next_stack{mparse_action_0(stack_, offset)}"),
        std::string::npos
    );
    EXPECT_NE(generated.find("class mparse_parse_Root_generator"), std::string::npos);
    EXPECT_NE(generated.find("class mparse_parse_Root_vertex_0_generator"), std::string::npos);
    EXPECT_NE(generated.find("class SequentialPartialGenerator"), std::string::npos);
    EXPECT_NE(generated.find("class SinglePartialGenerator"), std::string::npos);
    EXPECT_NE(
        generated.find("std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_"),
        std::string::npos
    );
    EXPECT_NE(generated.find("while (auto result = generator.next())"), std::string::npos);
    EXPECT_EQ(generated.find("std::vector<mparse_generated_detail::Result<int>>"), std::string::npos);
    EXPECT_EQ(generated.find("std::vector<mparse_generated_detail::PartialResult>"), std::string::npos);
    EXPECT_EQ(generated.find("appendAll"), std::string::npos);
    EXPECT_EQ(generated.find("parseAll"), std::string::npos);
}

TEST(CodegenCpp, EmitsNestedActionTrees) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "S",
                .type = "std::string",
                .rules = {
                    mparse::tests::rule(
                        {
                            mparse::tests::symbolItem("A"),
                            mparse::tests::symbolItem("B"),
                            mparse::tests::symbolItem("C"),
                        },
                        "$$ = $1 + $2 + $3"
                    ),
                },
            },
            mparse::spec::Symbol{
                .name = "A",
                .type = "std::string",
                .rules = {
                    mparse::tests::emptyRule("$$ = makeA()"),
                },
            },
            mparse::spec::Symbol{
                .name = "B",
                .type = "std::string",
                .rules = {
                    mparse::tests::emptyRule("$$ = makeB()"),
                },
            },
            mparse::spec::Symbol{
                .name = "C",
                .type = "std::string",
                .rules = {
                    mparse::tests::rule({mparse::tests::literalItem("c")}, "$$ = $1"),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);

    EXPECT_NE(generated.find("std::vector<std::any> resolved_args;"), std::string::npos);
    EXPECT_NE(generated.find("resolved_args.push_back(mparse_action_"), std::string::npos);
    EXPECT_NE(
        generated.find("mparse_generated_detail::semanticValue<std::string>(resolved_args, 0)"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("mparse_generated_detail::semanticValue<std::string>(resolved_args, 1)"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("mparse_generated_detail::semanticValue<std::string>(resolved_args, 2)"),
        std::string::npos
    );
}

TEST(CodegenCpp, EmitsRepeatedLiteralTransitions) {
    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "Root",
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

    const auto generated = generateCpp(specification);

    EXPECT_NE(
        generated.find("const auto repeat_count = (offset);"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("std::cmp_less(repeat_count, 0)"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("position_ + repeated_size"),
        std::string::npos
    );
}

TEST(CodegenCpp, EmitsRegexTransitions) {
    const auto identifier_regex = mparse::spec::RegexExpression{
        .value = mparse::spec::RegexSequence{
            .items = {
                regexExpression(mparse::spec::RegexRange{.from = 'a', .to = 'z'}),
                regexExpression(mparse::spec::RegexRepeat{
                    .item = regexExpression(mparse::spec::RegexAlternative{
                        .alternatives = {
                            regexExpression(mparse::spec::RegexRange{.from = 'a', .to = 'z'}),
                            regexExpression(mparse::spec::RegexRange{.from = '0', .to = '9'}),
                        },
                    }),
                    .kind = mparse::spec::RegexRepeatKind::ZeroOrMore,
                }),
            },
        },
    };

    const auto specification = mparse::spec::Specification{
        .symbols = {
            mparse::spec::Symbol{
                .name = "Identifier",
                .type = "std::string",
                .rules = {
                    mparse::tests::rule(
                        {
                            mparse::spec::RuleItem{
                                .value = mparse::spec::RuleItemRegex{
                                    .expression = identifier_regex,
                                },
                            },
                        },
                        "$$ = $1"
                    ),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);

    EXPECT_NE(generated.find("class RegexEdgeGenerator"), std::string::npos);
    EXPECT_NE(generated.find("auto regex_match_"), std::string::npos);
    EXPECT_NE(
        generated.find("mparse_generated_detail::pushUnique(result, position);"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("next_stack.push_back(std::string{input.substr(position, match_position - position)})"),
        std::string::npos
    );
    EXPECT_NE(
        generated.find("mparse_generated_detail::semanticValue<std::string>(args, 0)"),
        std::string::npos
    );
}
