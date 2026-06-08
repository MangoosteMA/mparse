#pragma once

#include <spec/data.h>

#include <optional>
#include <string>
#include <vector>

namespace mparse::tests {

    spec::RuleItem symbolItem(std::string name);
    spec::RuleItem literalItem(std::string value);
    spec::RuleItem rangeItem(char from, char to);

    spec::Rule rule(
        std::vector<spec::RuleItem> items = {},
        std::optional<std::string> action = std::nullopt
    );

    spec::Rule emptyRule(std::optional<std::string> action = std::nullopt);

    spec::Symbol symbol(
        std::string name,
        std::vector<spec::Rule> rules,
        spec::SourceReference source_reference = {}
    );

    spec::Specification specification(std::vector<spec::Symbol> symbols);

    const spec::RuleItemSymbol& asSymbolItem(const spec::RuleItem& item);

} // namespace mparse::tests
