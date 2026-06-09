#include "error.h"

#include <analysis/algo/find_empty_end_cycles.h>
#include <analysis/algo/resolve_empty_cycles.h>
#include <analysis/symbol.h>
#include <mparse/utils.h>

#include <string_view>
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

        AnalysisErrorSourceReference makeSourceReference(const Symbol& symbol) {
            return AnalysisErrorSourceReference{
                .name = symbol.originalName(),
                .source_reference = symbol.originalSymbol().source_reference,
            };
        }

        std::string formatCount(size_t count, std::string_view singular_name) {
            auto result = std::to_string(count) + " " + std::string{singular_name};
            if (count != 1) {
                result += "s";
            }
            return result;
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
            makeSourceReference(symbol),
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

    AnalysisError makeUnknownSymbolReferenceError(
        const Symbol& source_symbol,
        const std::string& referenced_symbol_name
    ) {
        std::vector<AnalysisErrorSourceReference> source_references{
            makeSourceReference(source_symbol),
        };

        auto message = "unknown symbol reference " + referenced_symbol_name +
                       " in " +
                       source_references.front().formatWithSourceReference();

        return AnalysisError(
            AnalysisErrorKind::UnknownSymbolReference,
            std::move(message),
            std::move(source_references)
        );
    }

    AnalysisError makeInvalidSymbolArgumentCountError(
        const Symbol& source_symbol,
        const Symbol& target_symbol,
        size_t actual_count
    ) {
        std::vector<AnalysisErrorSourceReference> source_references{
            makeSourceReference(source_symbol),
            makeSourceReference(target_symbol),
        };

        const auto expected_count = target_symbol.arguments().size();
        auto message = "invalid argument count for symbol reference " +
                       target_symbol.originalName() + " in " +
                       source_references.front().formatWithSourceReference() +
                       ": " +
                       source_references.back().formatWithSourceReference() +
                       " expects " + formatCount(expected_count, "argument") +
                       ", got " + formatCount(actual_count, "argument");

        return AnalysisError(
            AnalysisErrorKind::InvalidSymbolArgumentCount,
            std::move(message),
            std::move(source_references)
        );
    }

} // namespace mparse::analysis
