#include "symbol.h"

namespace mparse::analysis {

    Symbol::Symbol(spec::Symbol symbol)
        : symbol_{std::move(symbol)}
        , original_symbol_{symbol_} {
        for (auto&& rule : symbol_.rules) {
            rules_.push_back(Rule(std::move(rule)));
        }
        symbol_.rules.clear();
        original_symbol_.rules.clear();
    }

    Symbol Symbol::cloneWithoutRules(Symbol symbol) {
        symbol.rules_.clear();
        return symbol;
    }

    Symbol& Symbol::setName(std::string name) {
        symbol_.name = std::move(name);
        return *this;
    }

    Symbol& Symbol::setRules(std::vector<Rule> rules) {
        rules_ = std::move(rules);
        return *this;
    }

    const std::string& Symbol::name() const {
        return symbol_.name;
    }

    const spec::SourceReference& Symbol::sourceReference() const {
        return symbol_.source_reference;
    }

    const spec::Symbol& Symbol::originalSymbol() const {
        return original_symbol_;
    }

    const std::string& Symbol::originalName() const {
        return original_symbol_.name;
    }

    const std::optional<std::string>& Symbol::type() const {
        return symbol_.type;
    }

    const std::vector<spec::SymbolArgument>& Symbol::arguments() const {
        return symbol_.arguments;
    }

    const std::vector<Rule>& Symbol::rules() const {
        return rules_;
    }

    std::string Symbol::formatWithSourceReference() const {
        return symbol_.formatWithSourceReference();
    }

    std::string Symbol::formatOriginalWithSourceReference() const {
        return original_symbol_.formatWithSourceReference();
    }

} // namespace mparse::analysis
