#pragma once

#include <analysis/symbol.h>

#include <string>
#include <string_view>

namespace mparse::codegen::cpp {

    std::string cppIdentifier(std::string_view value);
    std::string symbolBaseName(const analysis::SymbolPtr& symbol);
    std::string parseFunctionName(const analysis::SymbolPtr& symbol);
    std::string vertexFunctionName(
        const analysis::SymbolPtr& symbol,
        size_t vertex_index
    );
    std::string actionFunctionName(size_t action_index);
    std::string escapeStringLiteral(std::string_view value);
    std::string escapeCharLiteral(char value);

} // namespace mparse::codegen::cpp
