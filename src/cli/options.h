#pragma once

#include <filesystem>
#include <string>

namespace mparse {

    enum class GenerationLanguage {
        Cpp,
    };

    struct CLIOptions {
        std::filesystem::path grammar;
        std::filesystem::path output;
        std::string root_symbol_name;
        GenerationLanguage language = GenerationLanguage::Cpp;
        bool check_nonprogressing_cycles = true;
        int verbose = 0;
    };

    CLIOptions parseArguments(int argc, char* argv[]) noexcept;

} // namespace mparse
