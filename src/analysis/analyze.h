#pragma once

#include <analysis/error.h>
#include <analysis/symbols_storage.h>
#include <spec/data.h>

namespace mparse::analysis {

    struct AnalysisOptions {
        bool check_nonprogressing_cycles = true;
    };

    SymbolsStorage analyzeOrThrow(
        const spec::Specification& specification,
        AnalysisOptions options = {}
    );

    SymbolsStorage analyze(
        const spec::Specification& specification,
        AnalysisOptions options = {}
    ) noexcept;

} // namespace mparse::analysis
