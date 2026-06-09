#pragma once

// Bootstrap parser API for src/spec/spec.grammar.
//
// The implementation is generated into generated_parser.cpp. Keep this header
// stable so the generated snapshot can be used by the regular build.

#include <spec/data.h>

#include <optional>
#include <string_view>
#include <vector>

namespace mparse::spec::generated {

    std::optional<std::vector<mparse::spec::Symbol>> parseSymbols(std::string_view source);

} // namespace mparse::spec::generated
