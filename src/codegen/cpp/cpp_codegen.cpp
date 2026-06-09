#include "cpp_codegen.h"

#include <codegen/cpp/render_model.h>
#include <codegen/cpp/renderer.h>
#include <codegen/cpp/templates.h>
#include <mparse/utils.h>

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace mparse::codegen::cpp {

    namespace {

        std::string rootSymbolName(
            const spec::Specification& specification,
            std::string_view requested_root_symbol_name
        ) {
            if (!requested_root_symbol_name.empty()) {
                return std::string{requested_root_symbol_name};
            }

            VERIFY(!specification.symbols.empty());
            return specification.symbols.front().name;
        }

    } // namespace

    std::string generateCpp(
        const spec::Specification& specification,
        const analysis::SymbolsStorage& symbols_storage,
        const analysis::GrammarAutomata& automata,
        std::string_view root_symbol_name
    ) {
        const auto root_name = rootSymbolName(specification, root_symbol_name);
        const auto root_symbol = symbols_storage.getSymbolByName(root_name);

        return renderTemplate(
            cppFileTemplate(),
            buildRenderModel(specification, automata, root_symbol)
        );
    }

} // namespace mparse::codegen::cpp
