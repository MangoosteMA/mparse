#pragma once

#include <analysis/automaton.h>
#include <codegen/cpp/writer.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace mparse::codegen::cpp {

    struct ActionFunctionSignature {
        std::string result_type;
        std::vector<std::string> input_types;
        std::string parameters_declaration;
        std::string parameters_forwarding;
    };

    using ActionNodeIds =
        std::unordered_map<const analysis::ActionTreeNode*, size_t>;
    using ActionFunctionSignatures =
        std::unordered_map<const analysis::ActionTreeNode*, ActionFunctionSignature>;

    void emitActionFunctionForwardDeclaration(
        Writer& writer,
        size_t action_index,
        const ActionFunctionSignature& signature
    );

    void emitActionFunction(
        Writer& writer,
        size_t action_index,
        const analysis::ActionTreeNode& action,
        const ActionNodeIds& action_node_ids,
        const ActionFunctionSignatures& action_signatures
    );

} // namespace mparse::codegen::cpp
