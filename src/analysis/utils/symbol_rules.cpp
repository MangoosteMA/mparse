#include "symbol_rules.h"

#include <analysis/utils/rules.h>
#include <mparse/utils.h>

#include <algorithm>
#include <variant>

namespace mparse::analysis {

    namespace {

        const spec::RuleItemSymbol* firstRuleItemSymbol(const Rule& rule) {
            if (rule.items().empty()) {
                return nullptr;
            }
            return std::get_if<spec::RuleItemSymbol>(&rule.items().front().value);
        }

        bool canInlineSymbol(const SymbolPtr& symbol) {
            return VERIFY(symbol)->arguments().empty();
        }

    } // namespace

    bool canInlineFirstSymbolReference(const Rule& rule, const SymbolPtr& symbol) {
        const auto* first_symbol = firstRuleItemSymbol(rule);
        if (!first_symbol) {
            return false;
        }

        return first_symbol->name == VERIFY(symbol)->name() &&
               first_symbol->arguments.empty() &&
               canInlineSymbol(symbol);
    }

    bool hasInlinableRules(const SymbolPtr& source, const SymbolPtr& target) {
        return std::ranges::any_of(VERIFY(source)->rules(), [&](const Rule& rule) {
            return canInlineFirstSymbolReference(rule, target);
        });
    }

    std::vector<Rule> inlineFirstSymbolRules(
        const SymbolPtr& source,
        const SymbolPtr& target
    ) {
        VERIFY(source);
        VERIFY(target);

        std::vector<Rule> rules;

        for (const auto& rule : source->rules()) {
            if (!canInlineFirstSymbolReference(rule, target)) {
                rules.push_back(rule);
                continue;
            }

            for (const auto& target_rule : target->rules()) {
                rules.push_back(replaceFirstSymbolWithRule(rule, target_rule));
            }
        }

        return rules;
    }

} // namespace mparse::analysis
