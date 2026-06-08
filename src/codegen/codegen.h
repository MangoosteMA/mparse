#pragma once

#include <analysis/automaton.h>
#include <analysis/symbols_storage.h>
#include <spec/data.h>

#include <string>

namespace mparse::codegen {

    enum class Language {
        Cpp,
    };

    struct GenerationOptions {
        Language language = Language::Cpp;
        std::string root_symbol_name;
    };

    std::string generate(
        const spec::Specification& specification,
        const analysis::SymbolsStorage& symbols_storage,
        const analysis::GrammarAutomata& automata,
        const GenerationOptions& options
    );

} // namespace mparse::codegen
