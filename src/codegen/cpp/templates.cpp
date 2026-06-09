#include "templates.h"

namespace mparse::codegen::cpp {

    std::string_view cppFileTemplate() {
        return
#include "templates/cpp_file.cpp.inja.inc"
            ;
    }

    std::string_view runtimeTemplate() {
        return
#include "templates/runtime.cpp.inja.inc"
            ;
    }

    std::string_view actionFunctionTemplate() {
        return
#include "templates/action_function.cpp.inja.inc"
            ;
    }

    std::string_view parseGeneratorDeclarationTemplate() {
        return
#include "templates/parse_generator_declaration.cpp.inja.inc"
            ;
    }

    std::string_view vertexGeneratorDeclarationTemplate() {
        return
#include "templates/vertex_generator_declaration.cpp.inja.inc"
            ;
    }

    std::string_view parseGeneratorDefinitionTemplate() {
        return
#include "templates/parse_generator_definition.cpp.inja.inc"
            ;
    }

    std::string_view vertexGeneratorConstructorTemplate() {
        return
#include "templates/vertex_generator_constructor.cpp.inja.inc"
            ;
    }

    std::string_view vertexGeneratorMakeNextTemplate() {
        return
#include "templates/vertex_generator_make_next.cpp.inja.inc"
            ;
    }

    std::string_view literalEdgeGeneratorCaseTemplate() {
        return
#include "templates/literal_edge_generator_case.cpp.inja.inc"
            ;
    }

    std::string_view rangeEdgeGeneratorCaseTemplate() {
        return
#include "templates/range_edge_generator_case.cpp.inja.inc"
            ;
    }

    std::string_view repeatedLiteralEdgeGeneratorCaseTemplate() {
        return
#include "templates/repeated_literal_edge_generator_case.cpp.inja.inc"
            ;
    }

    std::string_view symbolEdgeGeneratorCaseTemplate() {
        return
#include "templates/symbol_edge_generator_case.cpp.inja.inc"
            ;
    }

    std::string_view reduceEdgeGeneratorCaseTemplate() {
        return
#include "templates/reduce_edge_generator_case.cpp.inja.inc"
            ;
    }

    std::string_view vertexGeneratorMakeEdgeTemplate() {
        return
#include "templates/vertex_generator_make_edge.cpp.inja.inc"
            ;
    }

    std::string_view rootParseFunctionTemplate() {
        return
#include "templates/root_parse_function.cpp.inja.inc"
            ;
    }

} // namespace mparse::codegen::cpp
