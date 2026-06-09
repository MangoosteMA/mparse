#include "validate_symbol_references.h"

#include <analysis/error.h>
#include <mparse/utils.h>

#include <string>
#include <unordered_map>
#include <variant>

namespace mparse::analysis {

    namespace {

        using SymbolsByName = std::unordered_map<std::string, SymbolPtr>;

        SymbolsByName makeSymbolsByName(const SymbolsStorage& symbols_storage) {
            SymbolsByName result;
            for (const auto& symbol : symbols_storage.symbols()) {
                result.emplace(VERIFY(symbol)->name(), symbol);
            }
            return result;
        }

        void validateSymbolReference(
            const Symbol& source_symbol,
            const spec::RuleItemSymbol& item_symbol,
            const SymbolsByName& symbols_by_name
        ) {
            const auto target = symbols_by_name.find(item_symbol.name);
            if (target == symbols_by_name.end()) {
                throw makeUnknownSymbolReferenceError(source_symbol, item_symbol.name);
            }

            const auto& target_symbol = *VERIFY(target->second);
            if (item_symbol.arguments.size() != target_symbol.arguments().size()) {
                throw makeInvalidSymbolArgumentCountError(
                    source_symbol,
                    target_symbol,
                    item_symbol.arguments.size()
                );
            }
        }

        void validateRuleItem(
            const Symbol& source_symbol,
            const spec::RuleItem& item,
            const SymbolsByName& symbols_by_name
        ) {
            if (const auto* item_symbol =
                    std::get_if<spec::RuleItemSymbol>(&item.value)) {
                validateSymbolReference(source_symbol, *item_symbol, symbols_by_name);
            }
        }

    } // namespace

    void validateSymbolReferences(const SymbolsStorage& symbols_storage) {
        const auto symbols_by_name = makeSymbolsByName(symbols_storage);

        for (const auto& symbol : symbols_storage.symbols()) {
            const auto& source_symbol = *VERIFY(symbol);
            for (const auto& rule : source_symbol.rules()) {
                for (const auto& item : rule.items()) {
                    validateRuleItem(source_symbol, item, symbols_by_name);
                }
            }
        }
    }

} // namespace mparse::analysis
