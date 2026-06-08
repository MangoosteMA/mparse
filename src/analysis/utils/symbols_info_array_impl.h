#pragma once

#include "symbols_info_array.h"

#include <mparse/utils.h>

namespace mparse::analysis {

    template <typename Info>
    SymbolsInfoArray<Info>::SymbolsInfoArray(const std::vector<SymbolPtr>& symbols) {
        for (const SymbolPtr& symbol : symbols) {
            symbol_name_to_info_.emplace(VERIFY(symbol)->name(), Info{});
        }
    }

    template <typename Info>
    Info& SymbolsInfoArray<Info>::operator[](SymbolPtr symbol) {
        return symbol_name_to_info_.at(VERIFY(symbol)->name());
    }

    template <typename Info>
    const Info& SymbolsInfoArray<Info>::operator[](SymbolPtr symbol) const {
        return symbol_name_to_info_.at(VERIFY(symbol)->name());
    }

    template <typename Info>
    Info& SymbolsInfoArray<Info>::operator[](const std::string& symbol_name) {
        return symbol_name_to_info_.at(symbol_name);
    }

    template <typename Info>
    const Info& SymbolsInfoArray<Info>::operator[](const std::string& symbol_name) const {
        return symbol_name_to_info_.at(symbol_name);
    }

} // namespace mparse::analysis
