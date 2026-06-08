#include "action_emitter.h"

#include <codegen/cpp/identifiers.h>
#include <mparse/utils.h>

#include <cctype>
#include <string>
#include <vector>

namespace mparse::codegen::cpp {

    namespace {

        std::string semanticValueExpression(
            size_t index,
            const std::vector<std::string>& argument_types
        ) {
            VERIFY(index > 0);
            VERIFY(index <= argument_types.size());
            return "mparse_generated_detail::semanticValue<" +
                   argument_types[index - 1] + ">(args, " +
                   std::to_string(index - 1) + ")";
        }

        std::string rewriteAction(
            std::string_view action,
            const std::vector<std::string>& argument_types
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
                result += semanticValueExpression(argument_index, argument_types);
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

    } // namespace

    void emitActionFunction(
        Writer& writer,
        size_t action_index,
        const analysis::ReduceTransition& reduce
    ) {
        writer.line(
            "static std::any " + actionFunctionName(action_index) +
            "(const std::vector<std::any>& args) {"
        );
        writer.indent();
        writer.line(reduce.result_type + " ret{};");

        const auto action = VERIFY(reduce.action);
        // TODO: support nested ActionTreeNode edges. For the first codegen MVP,
        // reduce transitions are emitted as one direct rule action over the
        // current semantic stack.
        if (action->action()) {
            auto rewritten_action = rewriteAction(*action->action(), reduce.argument_types);
            if (needsStatementTerminator(rewritten_action)) {
                rewritten_action.push_back(';');
            }
            writer.line(rewritten_action);
        }

        writer.line("return ret;");
        writer.unindent();
        writer.line("}");
    }

} // namespace mparse::codegen::cpp
