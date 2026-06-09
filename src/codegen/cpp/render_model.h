#pragma once

#include <analysis/automaton.h>
#include <analysis/symbol.h>
#include <spec/data.h>

#include <nlohmann/json_fwd.hpp>

namespace mparse::codegen::cpp {

    nlohmann::json buildRenderModel(
        const spec::Specification& specification,
        const analysis::GrammarAutomata& automata,
        const analysis::SymbolPtr& root_symbol
    );

} // namespace mparse::codegen::cpp
