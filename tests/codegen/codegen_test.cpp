#include <analysis/analyze.h>
#include <codegen/codegen.h>
#include <utils/canon.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

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
