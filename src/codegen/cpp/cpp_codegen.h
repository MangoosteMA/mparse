#pragma once

#include <analysis/automaton.h>
#include <analysis/symbols_storage.h>
#include <spec/data.h>

#include <string>
#include <string_view>

namespace mparse::codegen::cpp {

    std::string generateCpp(
        const spec::Specification& specification,
        const analysis::SymbolsStorage& symbols_storage,
        const analysis::GrammarAutomata& automata,
        std::string_view root_symbol_name
    );

} // namespace mparse::codegen::cpp
