#pragma once

#include <string_view>

namespace mparse::codegen::cpp {

    std::string_view cppFileTemplate();
    std::string_view runtimeTemplate();

    std::string_view actionFunctionTemplate();
    std::string_view parseGeneratorDeclarationTemplate();
    std::string_view vertexGeneratorDeclarationTemplate();
    std::string_view parseGeneratorDefinitionTemplate();
    std::string_view vertexGeneratorConstructorTemplate();
    std::string_view vertexGeneratorMakeNextTemplate();
    std::string_view literalEdgeGeneratorCaseTemplate();
    std::string_view rangeEdgeGeneratorCaseTemplate();
    std::string_view repeatedLiteralEdgeGeneratorCaseTemplate();
    std::string_view regexEdgeGeneratorCaseTemplate();
    std::string_view symbolEdgeGeneratorCaseTemplate();
    std::string_view reduceEdgeGeneratorCaseTemplate();
    std::string_view vertexGeneratorMakeEdgeTemplate();
    std::string_view rootParseFunctionTemplate();

} // namespace mparse::codegen::cpp
