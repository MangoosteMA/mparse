#pragma once

#include <filesystem>

namespace mparse {

    enum class GenerationLanguage {
        Cpp,
    };

    struct CLIOptions {
        std::filesystem::path grammar;
        std::filesystem::path output;
        GenerationLanguage language = GenerationLanguage::Cpp;
        bool check_nonprogressing_cycles = true;
        int verbose = 0;
    };

    CLIOptions parseArguments(int argc, char* argv[]) noexcept;

} // namespace mparse
