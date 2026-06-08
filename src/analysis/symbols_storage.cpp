#include "symbols_storage.h"

#include <analysis/automaton.h>
#include <mparse/utils.h>

#include <algorithm>

namespace mparse::analysis {

    std::vector<SymbolPtr> SymbolsStorage::symbols() const {
        std::vector<SymbolPtr> symbols;
        symbols.reserve(name_to_symbol_.size());

        for (const auto& [_, symbol] : name_to_symbol_) {
            symbols.push_back(symbol);
        }

        std::ranges::sort(
            symbols,
            std::ranges::less{},
            [](const SymbolPtr& symbol) -> const std::string& {
                return symbol->name();
            }
        );

        return symbols;
    }

    SymbolPtr SymbolsStorage::appendNewSymbol(Symbol symbol) {
        auto symbol_ptr = std::make_shared<Symbol>(std::move(symbol));
        const auto [iterator, inserted] =
            name_to_symbol_.emplace(symbol_ptr->name(), symbol_ptr);

        VERIFY(inserted);
        return iterator->second;
    }

    SymbolPtr SymbolsStorage::getSymbolByName(const std::string& name) const {
        return name_to_symbol_.at(name);
    }

    std::string SymbolsStorage::generateUnexistingName(const std::string& name_prefix) const {
        std::string name = name_prefix;
        size_t index = 0;

        while (name_to_symbol_.contains(name)) {
            name = name_prefix + std::to_string(index);
            index++;
        }

        return name;
    }

    GrammarAutomata SymbolsStorage::buildAutomata() const {
        return mparse::analysis::buildAutomata(*this);
    }

    SymbolsStorage makeSymbolsStorageFromSpecification(const spec::Specification& specification) {
        SymbolsStorage symbols_storage;
        for (const auto& symbol : specification.symbols) {
            symbols_storage.appendNewSymbol(Symbol(symbol));
        }
        return symbols_storage;
    }

} // namespace mparse::analysis
