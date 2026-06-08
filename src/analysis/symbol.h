#pragma once

#include <analysis/rule.h>
#include <spec/data.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mparse::analysis {

    class Symbol {
    public:
        Symbol(spec::Symbol symbol);

        static Symbol cloneWithoutRules(Symbol symbol);
        Symbol& setName(std::string name);
        Symbol& setRules(std::vector<Rule> rules);

        const std::string& name() const;
        const spec::SourceReference& sourceReference() const;
        const spec::Symbol& originalSymbol() const;
        const std::string& originalName() const;
        const std::optional<std::string>& type() const;
        const std::vector<spec::SymbolArgument>& arguments() const;
        const std::vector<Rule>& rules() const;
        std::string formatWithSourceReference() const;
        std::string formatOriginalWithSourceReference() const;

    private:
        spec::Symbol symbol_;
        spec::Symbol original_symbol_;
        std::vector<Rule> rules_;
    };

    using SymbolPtr = std::shared_ptr<Symbol>;

} // namespace mparse::analysis
