#pragma once

#include <analysis/symbol.h>

#include <vector>

namespace mparse::analysis {

    bool canInlineFirstSymbolReference(const Rule& rule, const SymbolPtr& symbol);
    bool hasInlinableRules(const SymbolPtr& source, const SymbolPtr& target);

    std::vector<Rule> inlineFirstSymbolRules(
        const SymbolPtr& source,
        const SymbolPtr& target
    );

} // namespace mparse::analysis
