#include <analysis/analyze.h>
#include <analysis/statistics.h>
#include <cli/options.h>
#include <codegen/codegen.h>
#include <spec/spec.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

    mparse::codegen::Language toCodegenLanguage(mparse::GenerationLanguage language) {
        switch (language) {
        case mparse::GenerationLanguage::Cpp:
            return mparse::codegen::Language::Cpp;
        }

        throw std::runtime_error("unsupported generation language");
    }

    void writeFile(const std::filesystem::path& path, std::string_view content) {
        std::ofstream output{path, std::ios::binary};
        if (!output) {
            throw std::runtime_error("failed to open output file: " + path.string());
        }

        output << content;
    }

} // namespace

int main(int argc, char* argv[]) {
    const auto options = mparse::parseArguments(argc, argv);
    const auto specification = mparse::spec::readSpecification(options.grammar);

    const auto analysis = mparse::analysis::analyze(
        specification,
        mparse::analysis::AnalysisOptions{
            .check_nonprogressing_cycles = options.check_nonprogressing_cycles,
        }
    );

    if (options.verbose > 0) {
        mparse::analysis::printGrammarStatistics(analysis, std::cerr, options.verbose);
    }

    const auto automata = analysis.buildAutomata();

    const auto generated = mparse::codegen::generate(
        specification,
        analysis,
        automata,
        mparse::codegen::GenerationOptions{
            .language = toCodegenLanguage(options.language),
            .root_symbol_name = specification.symbols.front().name,
        }
    );

    writeFile(options.output, specification.source_template.insert(generated));

    return 0;
}
