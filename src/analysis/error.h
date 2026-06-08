#pragma once

#include <spec/data.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace mparse::analysis {

    struct EmptyCycle;
    class Symbol;

    enum class AnalysisErrorKind {
        NonProgressingCycle,
        UnsupportedSelfStartingRule,
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
    AnalysisError makeUnsupportedSelfStartingRuleError(const Symbol& symbol);

} // namespace mparse::analysis
