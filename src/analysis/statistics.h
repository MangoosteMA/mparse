#pragma once

#include <analysis/symbols_storage.h>

#include <cstddef>
#include <iosfwd>
#include <spec/data.h>
#include <string>
#include <vector>

namespace mparse::analysis {

    enum class GrammarStatisticsMode {
        OriginalSymbols,
        ExpandedSymbols,
    };

    struct RuleStatistics {
        size_t items = 0;
    };

    struct SymbolStatistics {
        std::string name;
        spec::SourceReference source_reference;
        size_t rules = 0;
        size_t items = 0;
        std::vector<RuleStatistics> rule_statistics;

        std::string formatWithSourceReference() const;
    };

    struct GrammarStatistics {
        size_t symbols = 0;
        size_t rules = 0;
        size_t items = 0;
        std::vector<SymbolStatistics> symbol_statistics;
    };

    GrammarStatistics collectGrammarStatistics(
        const SymbolsStorage& symbols_storage,
        GrammarStatisticsMode mode = GrammarStatisticsMode::OriginalSymbols
    );

    void printGrammarStatistics(
        const SymbolsStorage& symbols_storage,
        std::ostream& output,
        int verbosity
    );

} // namespace mparse::analysis
