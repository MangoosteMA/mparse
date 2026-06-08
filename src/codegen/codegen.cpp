#include "codegen.h"

#include <codegen/cpp/cpp_codegen.h>
#include <mparse/utils.h>

namespace mparse::codegen {

    std::string generate(
        const spec::Specification& specification,
        const analysis::SymbolsStorage& symbols_storage,
        const analysis::GrammarAutomata& automata,
        const GenerationOptions& options
    ) {
        switch (options.language) {
            case Language::Cpp:
                return cpp::generateCpp(
                    specification,
                    symbols_storage,
                    automata,
                    options.root_symbol_name
                );
        }

        VERIFY(false);
    }

} // namespace mparse::codegen
