#include "action_emitter.h"

#include <codegen/cpp/identifiers.h>
#include <codegen/cpp/renderer.h>
#include <codegen/cpp/templates.h>
#include <mparse/utils.h>

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace mparse::codegen::cpp {

    namespace {

        std::string semanticValueExpression(
            size_t index,
            const std::vector<std::string>& argument_types,
            std::string_view args_name
        ) {
            VERIFY(index > 0);
            VERIFY(index <= argument_types.size());
            return "mparse_generated_detail::semanticValue<" +
                   argument_types[index - 1] + ">(" + std::string{args_name} + ", " +
                   std::to_string(index - 1) + ")";
        }

        std::string rewriteAction(
            std::string_view action,
            const std::vector<std::string>& argument_types,
            std::string_view args_name
        ) {
            std::string result;

            for (size_t index = 0; index < action.size();) {
                if (action[index] != '$') {
                    result.push_back(action[index]);
                    index++;
                    continue;
                }

                if (index + 1 < action.size() && action[index + 1] == '$') {
                    result += "ret";
                    index += 2;
                    continue;
                }

                size_t digit_begin = index + 1;
                size_t digit_end = digit_begin;
                while (digit_end < action.size() &&
                       std::isdigit(static_cast<unsigned char>(action[digit_end])) != 0) {
                    digit_end++;
                }

                if (digit_end == digit_begin) {
                    result.push_back(action[index]);
                    index++;
                    continue;
                }

                const auto argument_index = std::stoul(std::string{
                    action.substr(digit_begin, digit_end - digit_begin),
                });
                result += semanticValueExpression(argument_index, argument_types, args_name);
                index = digit_end;
            }

            return result;
        }

        bool needsStatementTerminator(std::string_view statement) {
            while (!statement.empty() &&
                   std::isspace(static_cast<unsigned char>(statement.back())) != 0) {
                statement.remove_suffix(1);
            }

            return statement.empty() || statement.back() != ';';
        }

        std::string indent(std::string_view text, std::string_view indentation) {
            std::string result;
            size_t line_begin = 0;
            while (line_begin < text.size()) {
                const auto line_end = text.find('\n', line_begin);
                const auto line = text.substr(
                    line_begin,
                    line_end == std::string_view::npos ? std::string_view::npos : line_end - line_begin
                );

                result += indentation;
                result += line;
                result += '\n';

                if (line_end == std::string_view::npos) {
                    break;
                }
                line_begin = line_end + 1;
            }
            return result;
        }

        std::vector<std::string> actionArgumentTypes(
            const analysis::ActionTreeNode& action,
            const std::vector<std::string>& input_types,
            const ActionFunctionSignatures& action_signatures
        ) {
            std::vector<std::string> result;
            size_t cursor = 0;

            for (const auto& edge : action.edges()) {
                VERIFY(edge.offset_left <= edge.offset_right);
                VERIFY(edge.offset_right <= input_types.size());
                VERIFY(cursor <= edge.offset_left);

                result.insert(
                    result.end(),
                    input_types.begin() + static_cast<std::ptrdiff_t>(cursor),
                    input_types.begin() + static_cast<std::ptrdiff_t>(edge.offset_left)
                );

                const auto& edge_signature =
                    action_signatures.at(VERIFY(edge.node).get());
                result.push_back(edge_signature.result_type);
                cursor = edge.offset_right;
            }

            result.insert(
                result.end(),
                input_types.begin() + static_cast<std::ptrdiff_t>(cursor),
                input_types.end()
            );
            return result;
        }

        std::string emitPushArgument(size_t argument_index, std::string_view target) {
            return std::string{target} + ".push_back(args.at(" +
                   std::to_string(argument_index) + "));\n";
        }

        std::string transformedArgsStatement(
            const analysis::ActionTreeNode& action,
            const std::vector<std::string>& input_types,
            const ActionNodeIds& action_node_ids,
            const ActionFunctionSignatures& action_signatures
        ) {
            std::string result;
            const auto argument_types =
                actionArgumentTypes(action, input_types, action_signatures);

            result += "std::vector<std::any> resolved_args;\n";
            result += "resolved_args.reserve(" + std::to_string(argument_types.size()) + ");\n";

            size_t cursor = 0;
            for (const auto& edge : action.edges()) {
                for (size_t argument_index = cursor; argument_index < edge.offset_left; argument_index++) {
                    result += emitPushArgument(argument_index, "resolved_args");
                }

                result += "{\n";
                result += "    std::vector<std::any> nested_args;\n";
                result += "    nested_args.reserve(" +
                          std::to_string(edge.offset_right - edge.offset_left) +
                          ");\n";
                for (size_t argument_index = edge.offset_left; argument_index < edge.offset_right; argument_index++) {
                    result += "    " + emitPushArgument(argument_index, "nested_args");
                }
                result += "    resolved_args.push_back(" +
                          actionFunctionName(action_node_ids.at(VERIFY(edge.node).get())) +
                          "(nested_args));\n";
                result += "}\n";

                cursor = edge.offset_right;
            }

            for (size_t argument_index = cursor; argument_index < input_types.size(); argument_index++) {
                result += emitPushArgument(argument_index, "resolved_args");
            }

            return result;
        }

        std::string ruleActionBody(
            const analysis::ActionTreeNode& action,
            const ActionFunctionSignature& signature,
            const ActionNodeIds& action_node_ids,
            const ActionFunctionSignatures& action_signatures
        ) {
            std::string body;
            std::string args_name = "args";
            std::vector<std::string> argument_types = signature.input_types;

            if (!action.edges().empty()) {
                body += transformedArgsStatement(
                    action,
                    signature.input_types,
                    action_node_ids,
                    action_signatures
                );
                args_name = "resolved_args";
                argument_types = actionArgumentTypes(
                    action,
                    signature.input_types,
                    action_signatures
                );
            }

            body += signature.result_type + " ret{};\n";

            if (action.action()) {
                auto rewritten_action = rewriteAction(*action.action(), argument_types, args_name);
                if (needsStatementTerminator(rewritten_action)) {
                    rewritten_action.push_back(';');
                }
                body += rewritten_action + "\n";
            }

            body += "return ret;\n";
            return indent(body, "    ");
        }

        std::string forwardSemanticValueBody(
            const analysis::ActionTreeNode& action,
            const ActionFunctionSignature& signature,
            const ActionNodeIds& action_node_ids,
            const ActionFunctionSignatures& action_signatures
        ) {
            if (action.edges().empty()) {
                VERIFY(signature.input_types.size() == 1);
                return "    return args.at(0);\n";
            }

            const auto argument_types =
                actionArgumentTypes(action, signature.input_types, action_signatures);
            VERIFY(argument_types.size() == 1);

            auto body = transformedArgsStatement(
                action,
                signature.input_types,
                action_node_ids,
                action_signatures
            );
            body += "return resolved_args.at(0);\n";
            return indent(body, "    ");
        }

    } // namespace

    void emitActionFunctionForwardDeclaration(
        Writer& writer,
        size_t action_index
    ) {
        writer.line(
            "static std::any " + actionFunctionName(action_index) +
            "(const std::vector<std::any>& args);"
        );
    }

    void emitActionFunction(
        Writer& writer,
        size_t action_index,
        const analysis::ActionTreeNode& action,
        const ActionNodeIds& action_node_ids,
        const ActionFunctionSignatures& action_signatures
    ) {
        const auto& signature = action_signatures.at(&action);

        std::string body;
        switch (action.kind()) {
        case analysis::ActionTreeNodeKind::RuleAction:
            body = ruleActionBody(
                action,
                signature,
                action_node_ids,
                action_signatures
            );
            break;
        case analysis::ActionTreeNodeKind::ForwardSemanticValue:
            body = forwardSemanticValueBody(
                action,
                signature,
                action_node_ids,
                action_signatures
            );
            break;
        case analysis::ActionTreeNodeKind::EmptySymbolReference:
            VERIFY(false);
        }

        writer.write(renderTemplate(
            actionFunctionTemplate(),
            nlohmann::json{
                {"action_name", actionFunctionName(action_index)},
                {"body", body},
            }
        ));
    }

} // namespace mparse::codegen::cpp
