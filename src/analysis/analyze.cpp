#include "analyze.h"

#include <analysis/algo/expand_empty_items.h>
#include <analysis/algo/find_empty_end_cycles.h>
#include <analysis/algo/resolve_empty_cycles.h>
#include <analysis/error.h>
#include <analysis/symbols_storage.h>
#include <mparse/utils.h>

#include <exception>
#include <utility>

namespace mparse::analysis {

    SymbolsStorage analyzeOrThrow(
        const spec::Specification& specification,
        AnalysisOptions options
    ) {
        auto symbols_storage = makeSymbolsStorageFromSpecification(specification);

        if (options.check_nonprogressing_cycles) {
            symbols_storage = expandEmptyItems(symbols_storage);

            auto resolved_empty_cycles = resolveEmptyCycles(std::move(symbols_storage));
            symbols_storage = std::move(resolved_empty_cycles.symbols_storage);

            if (resolved_empty_cycles.unresolved_cycle) {
                throw makeNonProgressingCycleError(
                    *resolved_empty_cycles.unresolved_cycle
                );
            }

            if (auto empty_end_cycle = findEmptyEndCycle(symbols_storage)) {
                throw makeEmptyEndCycleError(*empty_end_cycle);
            }
        }

        // TODO: do more checks and analysis

        return symbols_storage;
    }

    SymbolsStorage analyze(
        const spec::Specification& specification,
        AnalysisOptions options
    ) noexcept {
        try {
            return analyzeOrThrow(specification, options);
        } catch (const std::exception& error) {
            mparse::exitWithError(error);
        }
    }

} // namespace mparse::analysis
