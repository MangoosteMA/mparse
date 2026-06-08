#pragma once

#include <analysis/symbols_storage.h>

#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    struct EmptyCycle {
        std::vector<SymbolPtr> symbols;
    };

    struct ResolveEmptyCyclesResult {
        SymbolsStorage symbols_storage;
        std::optional<EmptyCycle> unresolved_cycle;
    };

    ResolveEmptyCyclesResult resolveEmptyCycles(SymbolsStorage symbols_storage);
    std::string formatEmptyCycle(const EmptyCycle& cycle);

} // namespace mparse::analysis
