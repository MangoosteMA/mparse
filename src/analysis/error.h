#pragma once

#include <spec/data.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace mparse::analysis {

    struct EmptyCycle;
    struct EmptyEndCycle;
    class Symbol;

    enum class AnalysisErrorKind {
        NonProgressingCycle,
        EmptyEndCycle,
        UnsupportedSelfStartingRule,
        UnknownSymbolReference,
        InvalidSymbolArgumentCount,
    };

    struct AnalysisErrorSourceReference {
        std::string name;
        spec::SourceReference source_reference;

        std::string formatWithSourceReference() const;
    };

    class AnalysisError : public std::runtime_error {
    public:
        AnalysisError(
            AnalysisErrorKind kind,
            std::string message,
            std::vector<AnalysisErrorSourceReference> source_references
        );

        AnalysisErrorKind kind() const;
        const std::vector<AnalysisErrorSourceReference>& sourceReferences() const;

    private:
        AnalysisErrorKind kind_;
        std::vector<AnalysisErrorSourceReference> source_references_;
    };

    AnalysisError makeNonProgressingCycleError(const EmptyCycle& cycle);
    AnalysisError makeEmptyEndCycleError(const EmptyEndCycle& cycle);
    AnalysisError makeUnsupportedSelfStartingRuleError(const Symbol& symbol);

    AnalysisError makeUnknownSymbolReferenceError(
        const Symbol& source_symbol,
        const std::string& referenced_symbol_name
    );

    AnalysisError makeInvalidSymbolArgumentCountError(
        const Symbol& source_symbol,
        const Symbol& target_symbol,
        size_t actual_count
    );

} // namespace mparse::analysis
