#pragma once

#include <analysis/symbols_storage.h>

#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    struct EmptyEndCycle {
        std::vector<SymbolPtr> symbols;
    };

    std::optional<EmptyEndCycle> findEmptyEndCycle(
        const SymbolsStorage& symbols_storage
    );

    std::string formatEmptyEndCycle(const EmptyEndCycle& cycle);

} // namespace mparse::analysis
