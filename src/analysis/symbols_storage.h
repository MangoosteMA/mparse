#pragma once

#include <analysis/symbol.h>

#include <string>
#include <unordered_map>

namespace mparse::analysis {

    class GrammarAutomata;

    class SymbolsStorage {
    public:
        std::vector<SymbolPtr> symbols() const;

        SymbolPtr appendNewSymbol(Symbol symbol);
        SymbolPtr getSymbolByName(const std::string& name) const;
        std::string generateUnexistingName(const std::string& name_prefix = "") const;
        GrammarAutomata buildAutomata() const;

    private:
        std::unordered_map<std::string, SymbolPtr> name_to_symbol_;
    };

    SymbolsStorage makeSymbolsStorageFromSpecification(const spec::Specification& specification);

} // namespace mparse::analysis
