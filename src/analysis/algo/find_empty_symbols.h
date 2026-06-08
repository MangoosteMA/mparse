#pragma once

#include <analysis/utils/symbols_info_array.h>
#include <analysis/symbols_storage.h>

namespace mparse::analysis {

    // Stores true if symbol can be empty.
    SymbolsInfoArray<bool> findEmptySymbols(const SymbolsStorage& symbols_storage);

} // namespace mparse::analysis
