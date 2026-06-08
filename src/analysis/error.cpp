#include "error.h"

#include <analysis/algo/find_empty_end_cycles.h>
#include <analysis/algo/resolve_empty_cycles.h>
#include <analysis/symbol.h>
#include <mparse/utils.h>

#include <utility>

namespace mparse::analysis {

    namespace {

        std::string formatCycle(const std::vector<AnalysisErrorSourceReference>& symbols) {
            VERIFY(!symbols.empty());

            std::string result;
            for (const auto& symbol : symbols) {
                if (!result.empty()) {
                    result += " -> ";
                }
                result += symbol.formatWithSourceReference();
            }
            return result;
        }

        std::vector<AnalysisErrorSourceReference> makeSourceReferences(
            const EmptyCycle& cycle
        ) {
            std::vector<AnalysisErrorSourceReference> source_references;
            source_references.reserve(cycle.symbols.size());

            for (const auto& symbol : cycle.symbols) {
                source_references.push_back(AnalysisErrorSourceReference{
                    .name = symbol->name(),
                    .source_reference = symbol->sourceReference(),
                });
            }

            return source_references;
        }

    } // namespace

    std::string AnalysisErrorSourceReference::formatWithSourceReference() const {
        return spec::formatWithSourceReference(name, source_reference);
    }

    AnalysisError::AnalysisError(
        AnalysisErrorKind kind,
        std::string message,
        std::vector<AnalysisErrorSourceReference> source_references
    )
        : std::runtime_error(std::move(message))
        , kind_{kind}
        , source_references_{std::move(source_references)} {}

    AnalysisErrorKind AnalysisError::kind() const {
        return kind_;
    }

    const std::vector<AnalysisErrorSourceReference>& AnalysisError::sourceReferences() const {
        return source_references_;
    }

    AnalysisError makeNonProgressingCycleError(const EmptyCycle& cycle) {
        auto source_references = makeSourceReferences(cycle);
        auto message = "non-progressing cycle found: " + formatCycle(source_references);

        return AnalysisError(
            AnalysisErrorKind::NonProgressingCycle,
            std::move(message),
            std::move(source_references)
        );
    }

    AnalysisError makeEmptyEndCycleError(const EmptyEndCycle& cycle) {
        std::vector<AnalysisErrorSourceReference> source_references;
        source_references.reserve(cycle.symbols.size());

        for (const auto& symbol : cycle.symbols) {
            source_references.push_back(AnalysisErrorSourceReference{
                .name = symbol->name(),
                .source_reference = symbol->sourceReference(),
            });
        }

        auto message = "empty end-to-end automaton cycle found: " +
                       formatCycle(source_references);

        return AnalysisError(
            AnalysisErrorKind::EmptyEndCycle,
            std::move(message),
            std::move(source_references)
        );
    }

    AnalysisError makeUnsupportedSelfStartingRuleError(const Symbol& symbol) {
        std::vector<AnalysisErrorSourceReference> source_references{
            AnalysisErrorSourceReference{
                .name = symbol.originalName(),
                .source_reference = symbol.originalSymbol().source_reference,
            },
        };

        auto message = "unsupported self-starting rule found: " +
                       source_references.front().formatWithSourceReference() +
                       " starts with two references to " + symbol.originalName() +
                       "; rules like " + symbol.originalName() + " : " +
                       symbol.originalName() + " " + symbol.originalName() +
                       " are not supported yet";

        return AnalysisError(
            AnalysisErrorKind::UnsupportedSelfStartingRule,
            std::move(message),
            std::move(source_references)
        );
    }

} // namespace mparse::analysis
