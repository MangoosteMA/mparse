#include "options.h"

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace mparse {

    namespace {

        static const std::vector<std::pair<std::string, GenerationLanguage>> string_to_language{
            {"cpp", GenerationLanguage::Cpp},
            {"c++", GenerationLanguage::Cpp},
        };

        std::string buildLanguagesOptionsText() noexcept {
            static constexpr auto kSeparator = '|';

            std::string result;
            for (const auto& [language, _] : string_to_language) {
                if (!result.empty()) {
                    result.push_back(kSeparator);
                }
                result += language;
            }
            return result;
        }

    } // namespace

    CLIOptions parseArguments(int argc, char* argv[]) noexcept {
        CLIOptions options;
        bool allow_nonprogressing_cycles = false;

        // TODO: more accurate description
        CLI::App app{"grammar parser"};

        app.add_option("-g,--grammar", options.grammar, "Path to the input grammar file")
            ->required()
            ->check(CLI::ExistingFile);

        app.add_option("-o,--output", options.output, "Path to the output file")
            ->required();

        app.add_option(
            "-r,--root-symbol",
            options.root_symbol_name,
            "Root symbol for the generated parser"
        );

        app.add_option("-l,--language", options.language, "Target language for generated grammar")
            ->transform(CLI::CheckedTransformer(string_to_language, CLI::ignore_case))
            ->option_text(buildLanguagesOptionsText())
            ->default_str("cpp");

        app.add_flag(
            "--allow-non-progressing-cycles",
            allow_nonprogressing_cycles,
            "Allow unresolved grammar cycles that do not consume input"
        );

        app.add_flag(
            "-v,--verbose",
            options.verbose,
            "Print grammar statistics; repeat for per-symbol details and expanded-symbol debug output"
        );

        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& error) {
            std::exit(app.exit(error));
        }

        options.check_nonprogressing_cycles = !allow_nonprogressing_cycles;

        return options;
    }

} // namespace mparse
