#include "spec.h"

#include "parser.h"
#include "utils.h"

#include <mparse/utils.h>

#include <exception>
#include <utility>

namespace mparse::spec {

    Specification readSpecificationOrThrow(const std::filesystem::path& path) {
        auto extracted = extractGrammarBlock(readFile(path));

        Parser parser{
            extracted.grammar,
            extracted.first_line,
        };

        return Specification{
            .symbols = parser.parseSymbols(),
            .source_template = std::move(extracted.source_template),
        };
    }

    Specification readSpecification(const std::filesystem::path& path) noexcept {
        try {
            return readSpecificationOrThrow(path);
        } catch (const std::exception& error) {
            mparse::exitWithError(error);
        }
    }

} // namespace mparse::spec
