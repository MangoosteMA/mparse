#include <analysis/algo/expand_empty_items.h>
#include <analysis/statistics.h>
#include <analysis/symbols_storage.h>
#include <utils/canon.h>
#include <utils/spec_builders.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

namespace {

    mparse::spec::Specification makeStatisticsSpecification() {
        using namespace mparse::tests;

        return specification({
            symbol("B", {
                            rule({literalItem("b")}, "$$ = $1"),
                            rule({literalItem("c"), symbolItem("A")}, "$$ = makeB($1, $2)"),
                        }),
            symbol("A", {
                            emptyRule("$$ = makeA()"),
                            rule({literalItem("a")}, "$$ = $1"),
                        }),
        });
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

TEST(GrammarStatistics, CollectsTotalsAndPrintsSummary) {
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(makeStatisticsSpecification());

    const auto statistics = mparse::analysis::collectGrammarStatistics(symbols_storage);

    EXPECT_EQ(statistics.symbols, 2);
    EXPECT_EQ(statistics.rules, 4);
    EXPECT_EQ(statistics.items, 4);

    std::ostringstream output;
    mparse::analysis::printGrammarStatistics(symbols_storage, output, 1);

    expectMatchesCanon("analysis/canons/statistics/summary_verbose1.txt", output.str());
}

TEST(GrammarStatistics, PrintsPerSymbolDetailsAtHigherVerbosity) {
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(makeStatisticsSpecification());

    std::ostringstream output;
    mparse::analysis::printGrammarStatistics(symbols_storage, output, 2);

    expectMatchesCanon("analysis/canons/statistics/per_symbol_verbose2.txt", output.str());
}

TEST(GrammarStatistics, AggregatesExpandedSymbolsByOriginalSymbol) {
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(makeStatisticsSpecification());
    const auto expanded_symbols_storage = mparse::analysis::expandEmptyItems(symbols_storage);

    const auto statistics = mparse::analysis::collectGrammarStatistics(
        expanded_symbols_storage,
        mparse::analysis::GrammarStatisticsMode::OriginalSymbols
    );

    EXPECT_EQ(statistics.symbols, 2);
    EXPECT_EQ(statistics.rules, 6);
    EXPECT_EQ(statistics.items, 6);

    std::ostringstream output;
    mparse::analysis::printGrammarStatistics(expanded_symbols_storage, output, 2);

    expectMatchesCanon("analysis/canons/statistics/aggregated_expanded_verbose2.txt", output.str());
}

TEST(GrammarStatistics, PrintsExpandedSymbolsAtDebugVerbosity) {
    const auto symbols_storage =
        mparse::analysis::makeSymbolsStorageFromSpecification(makeStatisticsSpecification());
    const auto expanded_symbols_storage = mparse::analysis::expandEmptyItems(symbols_storage);

    std::ostringstream output;
    mparse::analysis::printGrammarStatistics(expanded_symbols_storage, output, 3);

    expectMatchesCanon("analysis/canons/statistics/expanded_symbols_verbose3.txt", output.str());
}
