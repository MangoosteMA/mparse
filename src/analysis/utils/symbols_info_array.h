#pragma once

#include <analysis/symbol.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace mparse::analysis {

    template <typename Info>
    class SymbolsInfoArray {
    public:
        SymbolsInfoArray(const std::vector<SymbolPtr>& symbols);

        Info& operator[](SymbolPtr symbol);
        const Info& operator[](SymbolPtr symbol) const;
        Info& operator[](const std::string& symbol_name);
        const Info& operator[](const std::string& symbol_name) const;

    private:
        std::unordered_map<std::string, Info> symbol_name_to_info_;
    };

} // namespace mparse::analysis

#include "symbols_info_array_impl.h"
