#include "data.h"

namespace mparse::spec {

    bool RuleItemLiteral::empty() const {
        return value.empty();
    }

    bool RuleItemRange::empty() const {
        return from > to;
    }

    bool RuleItemRepeatedLiteral::empty() const {
        return value.empty();
    }

    std::string SourceTemplate::insert(std::string_view content) const {
        std::string result;
        result.reserve(before_insertion.size() + content.size() + after_insertion.size());

        result += before_insertion;
        result += content;
        result += after_insertion;

        return result;
    }

    std::string formatWithSourceReference(
        std::string_view name,
        const SourceReference& source_reference
    ) {
        return std::string{name} + " at " + std::to_string(source_reference.line) + ":" +
               std::to_string(source_reference.column);
    }

    std::string Symbol::formatWithSourceReference() const {
        return spec::formatWithSourceReference(name, source_reference);
    }

} // namespace mparse::spec
