#include "spec_builders.h"

#include <utility>
#include <variant>

namespace mparse::tests {

    spec::RuleItem symbolItem(std::string name) {
        return spec::RuleItem{
            .value = spec::RuleItemSymbol{
                .name = std::move(name),
            },
        };
    }

    spec::RuleItem literalItem(std::string value) {
        return spec::RuleItem{
            .value = spec::RuleItemLiteral{
                .value = std::move(value),
            },
        };
    }

    spec::RuleItem rangeItem(char from, char to) {
        return spec::RuleItem{
            .value = spec::RuleItemRange{
                .from = from,
                .to = to,
            },
        };
    }

    spec::Rule rule(
        std::vector<spec::RuleItem> items,
        std::optional<std::string> action
    ) {
        return spec::Rule{
            .items = std::move(items),
            .action = std::move(action),
        };
    }

    spec::Rule emptyRule(std::optional<std::string> action) {
        return rule({}, std::move(action));
    }

    spec::Symbol symbol(
        std::string name,
        std::vector<spec::Rule> rules,
        spec::SourceReference source_reference
    ) {
        return spec::Symbol{
            .name = std::move(name),
            .source_reference = source_reference,
            .rules = std::move(rules),
        };
    }

    spec::Specification specification(std::vector<spec::Symbol> symbols) {
        return spec::Specification{
            .symbols = std::move(symbols),
        };
    }

    const spec::RuleItemSymbol& asSymbolItem(const spec::RuleItem& item) {
        return std::get<spec::RuleItemSymbol>(item.value);
    }

} // namespace mparse::tests
