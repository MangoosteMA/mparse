#include <analysis/analyze.h>
#include <analysis/error.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

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

    mparse::spec::Symbol parameterizedSymbol(
        std::string name,
        std::vector<mparse::spec::SymbolArgument> arguments,
        std::vector<mparse::spec::Rule> rules,
        mparse::spec::SourceReference source_reference = {}
    ) {
        return mparse::spec::Symbol{
            .name = std::move(name),
            .source_reference = source_reference,
            .arguments = std::move(arguments),
            .rules = std::move(rules),
        };
    }

} // namespace

TEST(SymbolReferenceValidation, RejectsUnknownSymbolReference) {
    using namespace mparse::tests;

    const auto specification = mparse::tests::specification({
        symbol("Root", {
                           rule({symbolItem("Missing")}, "$$ = $1"),
                       },
               mparse::spec::SourceReference{.line = 7, .column = 5}),
    });

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected unknown symbol reference error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(
            error.kind(),
            mparse::analysis::AnalysisErrorKind::UnknownSymbolReference
        );
        EXPECT_EQ(
            std::string{error.what()},
            "unknown symbol reference Missing in Root at 7:5"
        );

        ASSERT_EQ(error.sourceReferences().size(), 1);
        EXPECT_EQ(error.sourceReferences()[0].name, "Root");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 7);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 5);
    }
}

TEST(SymbolReferenceValidation, RejectsInvalidSymbolArgumentCount) {
    using namespace mparse::tests;

    const auto specification = mparse::spec::Specification{
        .symbols = {
            parameterizedSymbol(
                "Target",
                {
                    mparse::spec::SymbolArgument{
                        .name = "offset",
                        .type = "int",
                    },
                },
                {
                    rule({literalItem("x")}, "$$ = $1"),
                },
                mparse::spec::SourceReference{.line = 3, .column = 1}
            ),
            symbol("Root", {
                               rule(
                                   {symbolItem("Target", {"offset + 1", "extra"})},
                                   "$$ = $1"
                               ),
                           },
                   mparse::spec::SourceReference{.line = 10, .column = 2}),
        },
    };

    try {
        (void) mparse::analysis::analyzeOrThrow(specification);
        FAIL() << "expected invalid symbol argument count error";
    } catch (const mparse::analysis::AnalysisError& error) {
        EXPECT_EQ(
            error.kind(),
            mparse::analysis::AnalysisErrorKind::InvalidSymbolArgumentCount
        );
        EXPECT_EQ(
            std::string{error.what()},
            "invalid argument count for symbol reference Target in Root at "
            "10:2: Target at 3:1 expects 1 argument, got 2 arguments"
        );

        ASSERT_EQ(error.sourceReferences().size(), 2);
        EXPECT_EQ(error.sourceReferences()[0].name, "Root");
        EXPECT_EQ(error.sourceReferences()[0].source_reference.line, 10);
        EXPECT_EQ(error.sourceReferences()[0].source_reference.column, 2);
        EXPECT_EQ(error.sourceReferences()[1].name, "Target");
        EXPECT_EQ(error.sourceReferences()[1].source_reference.line, 3);
        EXPECT_EQ(error.sourceReferences()[1].source_reference.column, 1);
    }
}

TEST(SymbolReferenceValidation, AllowsMatchingSymbolArgumentCount) {
    using namespace mparse::tests;

    const auto specification = mparse::spec::Specification{
        .symbols = {
            parameterizedSymbol(
                "Target",
                {
                    mparse::spec::SymbolArgument{
                        .name = "offset",
                        .type = "int",
                    },
                },
                {
                    rule({literalItem("x")}, "$$ = $1"),
                }
            ),
            symbol("Root", {
                               rule(
                                   {symbolItem("Target", {"offset + 1"})},
                                   "$$ = $1"
                               ),
                           }),
        },
    };

    EXPECT_NO_THROW((void) mparse::analysis::analyzeOrThrow(specification));
}
