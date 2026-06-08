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
                    mparse::tests::rule(
                        {mparse::tests::symbolItem("Digit")},
                        "$$ = $1"
                    ),
                },
            },
        },
    };

    const auto generated = generateCpp(specification);
    expectMatchesCanon("codegen/canons/cpp/range_and_symbol_parser.cpp", generated);
}
