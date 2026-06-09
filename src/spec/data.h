#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mparse::spec {

    struct RuleItemLiteral {
        std::string value;

        bool empty() const;
    };

    struct RuleItemRange {
        char from;
        char to;

        bool empty() const;
    };

    struct RuleItemRepeatedLiteral {
        std::string value;
        std::string count_expression;

        bool empty() const;
    };

    struct RuleItemSymbol {
        std::string name;
        std::vector<std::string> arguments;
    };

    // TODO: support regex
    using RuleItemValue = std::variant<
        RuleItemLiteral,
        RuleItemRange,
        RuleItemRepeatedLiteral,
        RuleItemSymbol>;

    struct RuleItem {
        RuleItemValue value;
        // Extra data will be stored in the future here.
    };

    struct Rule {
        std::vector<RuleItem> items;
        std::optional<std::string> action;
    };

    struct SymbolArgument {
        std::string name;
        std::string type;
    };

    struct SourceReference {
        size_t line = 1;
        size_t column = 1;
    };

    std::string formatWithSourceReference(
        std::string_view name,
        const SourceReference& source_reference
    );

    struct Symbol {
        std::string name;
        SourceReference source_reference;
        std::optional<std::string> type;
        std::vector<SymbolArgument> arguments;
        std::vector<Rule> rules;

        std::string formatWithSourceReference() const;
    };

    struct SourceTemplate {
        std::string before_insertion;
        std::string after_insertion;

        std::string insert(std::string_view content) const;
    };

    struct Specification {
        std::vector<Symbol> symbols;
        SourceTemplate source_template;
    };

} // namespace mparse::spec
