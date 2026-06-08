#pragma once

#include "data.h"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mparse::spec {

    struct ExtractedGrammarBlock {
        std::string grammar;
        size_t first_line = 1;
        SourceTemplate source_template;
    };

    class ParseError : public std::runtime_error {
    public:
        ParseError(size_t line, size_t column, const std::string& message);
    };

    std::string trim(std::string_view value);
    std::string readFile(const std::filesystem::path& path);
    ExtractedGrammarBlock extractGrammarBlock(std::string source);

} // namespace mparse::spec
