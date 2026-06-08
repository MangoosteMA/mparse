#include "cpp_codegen.h"

#include <codegen/cpp/action_emitter.h>
#include <codegen/cpp/identifiers.h>
#include <codegen/cpp/writer.h>
#include <mparse/utils.h>

#include <algorithm>
#include <any>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mparse::codegen::cpp {

    namespace {

        using ReduceActionIds = std::unordered_map<const analysis::ReduceTransition*, size_t>;

        std::string symbolValueType(const analysis::SymbolPtr& symbol) {
            return VERIFY(symbol)->type().value_or("std::monostate");
        }

        std::string parametersDeclaration(const analysis::SymbolPtr& symbol) {
            std::string result;
            for (const auto& argument : VERIFY(symbol)->arguments()) {
                result += ", " + argument.type + " " + argument.name;
            }
            return result;
        }

        std::string parametersForwarding(const analysis::SymbolPtr& symbol) {
            std::string result;
            for (const auto& argument : VERIFY(symbol)->arguments()) {
                result += ", " + argument.name;
            }
            return result;
        }

        std::string parseFunctionSignature(const analysis::SymbolPtr& symbol) {
            return "static std::vector<mparse_generated_detail::Result<" +
                   symbolValueType(symbol) + ">> " + parseFunctionName(symbol) +
                   "(std::string_view input, size_t position" +
                   parametersDeclaration(symbol) + ")";
        }

        std::string vertexFunctionSignature(
            const analysis::SymbolPtr& symbol,
            size_t vertex_index
        ) {
            return "static std::vector<mparse_generated_detail::PartialResult> " +
                   vertexFunctionName(symbol, vertex_index) +
                   "(std::string_view input, size_t position, "
                   "std::vector<std::any> stack" +
                   parametersDeclaration(symbol) + ")";
        }

        std::string vertexCall(
            const analysis::SymbolPtr& symbol,
            size_t vertex_index,
            std::string_view position,
            std::string_view stack
        ) {
            return vertexFunctionName(symbol, vertex_index) +
                   "(input, " + std::string{position} + ", " +
                   std::string{stack} + parametersForwarding(symbol) + ")";
        }

        std::string rootSymbolName(
            const spec::Specification& specification,
            std::string_view requested_root_symbol_name
        ) {
            if (!requested_root_symbol_name.empty()) {
                return std::string{requested_root_symbol_name};
            }

            VERIFY(!specification.symbols.empty());
            return specification.symbols.front().name;
        }

        void collectReduceActions(
            const analysis::GrammarAutomata& automata,
            ReduceActionIds& action_ids
        ) {
            for (const auto& automaton : automata.symbols()) {
                for (const auto& vertex : automaton.vertices) {
                    for (const auto& edge : vertex.edges) {
                        const auto* reduce =
                            std::get_if<analysis::ReduceTransition>(&edge.transition);
                        if (!reduce) {
                            continue;
                        }

                        action_ids.emplace(reduce, action_ids.size());
                    }
                }
            }
        }

        std::string join(const std::vector<std::string>& values, std::string_view separator) {
            std::string result;
            for (const auto& value : values) {
                if (!result.empty()) {
                    result += separator;
                }
                result += value;
            }
            return result;
        }

        std::string escapeGrammarLiteral(std::string_view value) {
            std::string result;
            for (const char character : value) {
                switch (character) {
                    case '\\':
                        result += "\\\\";
                        break;
                    case '\'':
                        result += "\\'";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    default:
                        result.push_back(character);
                        break;
                }
            }
            return result;
        }

        std::string formatGrammarItem(const spec::RuleItem& item) {
            return std::visit(
                mparse::Overloaded{
                    [](const spec::RuleItemLiteral& literal) {
                        return "'" + escapeGrammarLiteral(literal.value) + "'";
                    },
                    [](const spec::RuleItemRange& range) {
                        return "'" + escapeGrammarLiteral(std::string{range.from}) +
                               "'-'" + escapeGrammarLiteral(std::string{range.to}) + "'";
                    },
                    [](const spec::RuleItemSymbol& symbol) {
                        auto result = symbol.name;
                        if (!symbol.arguments.empty()) {
                            result += "[" + join(symbol.arguments, ", ") + "]";
                        }
                        return result;
                    },
                },
                item.value
            );
        }

        std::string formatGrammarRule(const spec::Rule& rule) {
            std::vector<std::string> items;
            items.reserve(rule.items.size());
            for (const auto& item : rule.items) {
                items.push_back(formatGrammarItem(item));
            }

            auto result = std::string{"    :"};
            if (!items.empty()) {
                result += " " + join(items, " ");
            }
            if (rule.action) {
                result += " { " + *rule.action + " }";
            }
            return result;
        }

        std::string formatGrammarSymbolHeader(const spec::Symbol& symbol) {
            auto result = symbol.name;
            if (!symbol.arguments.empty()) {
                std::vector<std::string> arguments;
                arguments.reserve(symbol.arguments.size());
                for (const auto& argument : symbol.arguments) {
                    arguments.push_back(argument.name + " " + argument.type);
                }
                result += "[" + join(arguments, ", ") + "]";
            }
            if (symbol.type) {
                result += "<" + *symbol.type + ">";
            }
            return result;
        }

        void emitCommentedText(Writer& writer, std::string_view text) {
            size_t line_begin = 0;
            while (line_begin <= text.size()) {
                const auto line_end = text.find('\n', line_begin);
                const auto line = text.substr(
                    line_begin,
                    line_end == std::string_view::npos ? std::string_view::npos : line_end - line_begin
                );
                writer.line(line.empty() ? "//" : "// " + std::string{line});

                if (line_end == std::string_view::npos) {
                    break;
                }
                line_begin = line_end + 1;
            }
        }

        void emitSourceGrammarComment(
            Writer& writer,
            const spec::Specification& specification
        ) {
            writer.line("// Source grammar:");
            writer.line("//");
            for (size_t symbol_index = 0; symbol_index < specification.symbols.size(); symbol_index++) {
                const auto& symbol = specification.symbols[symbol_index];
                emitCommentedText(writer, formatGrammarSymbolHeader(symbol));
                for (const auto& rule : symbol.rules) {
                    emitCommentedText(writer, formatGrammarRule(rule));
                }
                if (symbol_index + 1 < specification.symbols.size()) {
                    writer.line("//");
                }
            }
            writer.line();
        }

        void emitRuntime(Writer& writer) {
            writer.line("#include <any>");
            writer.line("#include <cstddef>");
            writer.line("#include <iterator>");
            writer.line("#include <optional>");
            writer.line("#include <string>");
            writer.line("#include <string_view>");
            writer.line("#include <utility>");
            writer.line("#include <variant>");
            writer.line("#include <vector>");
            writer.line();
            writer.line("namespace mparse_generated_detail {");
            writer.indent();
            writer.line("template <typename Value>");
            writer.line("struct Result {");
            writer.indent();
            writer.line("size_t position = 0;");
            writer.line("Value value{};");
            writer.unindent();
            writer.line("};");
            writer.line();
            writer.line("struct PartialResult {");
            writer.indent();
            writer.line("size_t position = 0;");
            writer.line("std::vector<std::any> stack;");
            writer.unindent();
            writer.line("};");
            writer.line();
            writer.line("template <typename Value>");
            writer.line("const Value& semanticValue(const std::vector<std::any>& args, size_t index) {");
            writer.indent();
            writer.line("return std::any_cast<const Value&>(args.at(index));");
            writer.unindent();
            writer.line("}");
            writer.line();
            writer.line("template <typename Value>");
            writer.line("void appendAll(std::vector<Value>& target, std::vector<Value>&& source) {");
            writer.indent();
            writer.line("target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));");
            writer.unindent();
            writer.line("}");
            writer.unindent();
            writer.line("} // namespace mparse_generated_detail");
            writer.line();
        }

        void emitForwardDeclarations(
            Writer& writer,
            const analysis::GrammarAutomata& automata
        ) {
            for (const auto& automaton : automata.symbols()) {
                writer.line(parseFunctionSignature(automaton.symbol) + ";");
                for (size_t vertex_index = 0; vertex_index < automaton.vertices.size(); vertex_index++) {
                    writer.line(vertexFunctionSignature(automaton.symbol, vertex_index) + ";");
                }
            }
            writer.line();
        }

        void emitActions(
            Writer& writer,
            const analysis::GrammarAutomata& automata,
            const ReduceActionIds& action_ids
        ) {
            std::vector<const analysis::ReduceTransition*> actions(action_ids.size());
            for (const auto& [reduce, action_index] : action_ids) {
                actions.at(action_index) = reduce;
            }

            for (size_t action_index = 0; action_index < actions.size(); action_index++) {
                emitActionFunction(writer, action_index, *VERIFY(actions[action_index]));
                writer.line();
            }
        }

        void emitLiteralTransition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            const analysis::LiteralTransition& literal
        ) {
            writer.line("{");
            writer.indent();
            writer.line(
                "const std::string_view literal = \"" +
                escapeStringLiteral(literal.value) + "\";"
            );
            writer.line("if (input.substr(position, literal.size()) == literal) {");
            writer.indent();
            writer.line("auto next_stack = stack;");
            writer.line("next_stack.push_back(std::string{literal});");
            writer.line(
                "auto next_results = " +
                vertexCall(automaton.symbol, edge.target, "position + literal.size()", "std::move(next_stack)") +
                ";"
            );
            writer.line("mparse_generated_detail::appendAll(results, std::move(next_results));");
            writer.unindent();
            writer.line("}");
            writer.unindent();
            writer.line("}");
        }

        void emitRangeTransition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            const analysis::RangeTransition& range
        ) {
            writer.line("{");
            writer.indent();
            writer.line(
                "if (position < input.size() && input[position] >= '" +
                escapeCharLiteral(range.from) + "' && input[position] <= '" +
                escapeCharLiteral(range.to) + "') {"
            );
            writer.indent();
            writer.line("auto next_stack = stack;");
            writer.line("next_stack.push_back(input[position]);");
            writer.line(
                "auto next_results = " +
                vertexCall(automaton.symbol, edge.target, "position + 1", "std::move(next_stack)") +
                ";"
            );
            writer.line("mparse_generated_detail::appendAll(results, std::move(next_results));");
            writer.unindent();
            writer.line("}");
            writer.unindent();
            writer.line("}");
        }

        void emitSymbolTransition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            const analysis::SymbolTransition& symbol_transition
        ) {
            writer.line("{");
            writer.indent();

            std::string arguments;
            for (const auto& argument : symbol_transition.arguments) {
                arguments += ", " + argument;
            }

            writer.line(
                "for (const auto& nested : " +
                parseFunctionName(symbol_transition.symbol) +
                "(input, position" + arguments + ")) {"
            );
            writer.indent();
            writer.line("auto next_stack = stack;");
            writer.line("next_stack.push_back(nested.value);");
            writer.line(
                "auto next_results = " +
                vertexCall(automaton.symbol, edge.target, "nested.position", "std::move(next_stack)") +
                ";"
            );
            writer.line("mparse_generated_detail::appendAll(results, std::move(next_results));");
            writer.unindent();
            writer.line("}");
            writer.unindent();
            writer.line("}");
        }

        void emitReduceTransition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            const analysis::ReduceTransition& reduce,
            const ReduceActionIds& action_ids
        ) {
            writer.line("{");
            writer.indent();
            writer.line(
                "std::vector<std::any> next_stack{" +
                actionFunctionName(action_ids.at(&reduce)) + "(stack)};"
            );
            writer.line(
                "auto next_results = " +
                vertexCall(automaton.symbol, edge.target, "position", "std::move(next_stack)") +
                ";"
            );
            writer.line("mparse_generated_detail::appendAll(results, std::move(next_results));");
            writer.unindent();
            writer.line("}");
        }

        void emitTransition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            const ReduceActionIds& action_ids
        ) {
            std::visit(
                mparse::Overloaded{
                    [&](const analysis::LiteralTransition& literal) {
                        emitLiteralTransition(writer, automaton, edge, literal);
                    },
                    [&](const analysis::RangeTransition& range) {
                        emitRangeTransition(writer, automaton, edge, range);
                    },
                    [&](const analysis::SymbolTransition& symbol) {
                        emitSymbolTransition(writer, automaton, edge, symbol);
                    },
                    [&](const analysis::ReduceTransition& reduce) {
                        emitReduceTransition(writer, automaton, edge, reduce, action_ids);
                    },
                },
                edge.transition
            );
        }

        void emitParseFunction(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton
        ) {
            writer.line(parseFunctionSignature(automaton.symbol) + " {");
            writer.indent();
            writer.line("std::vector<mparse_generated_detail::Result<" + symbolValueType(automaton.symbol) + ">> results;");
            writer.line(
                "for (auto&& partial : " +
                vertexCall(automaton.symbol, automaton.start, "position", "{}") +
                ") {"
            );
            writer.indent();
            writer.line("if (partial.stack.empty()) {");
            writer.indent();
            writer.line("continue;");
            writer.unindent();
            writer.line("}");
            writer.line("results.push_back(mparse_generated_detail::Result<" + symbolValueType(automaton.symbol) + ">{");
            writer.indent();
            writer.line(".position = partial.position,");
            writer.line(".value = std::any_cast<" + symbolValueType(automaton.symbol) + ">(partial.stack.back()),");
            writer.unindent();
            writer.line("});");
            writer.unindent();
            writer.line("}");
            writer.line("return results;");
            writer.unindent();
            writer.line("}");
            writer.line();
        }

        void emitVertexFunction(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index,
            const ReduceActionIds& action_ids
        ) {
            writer.line(vertexFunctionSignature(automaton.symbol, vertex_index) + " {");
            writer.indent();
            writer.line("std::vector<mparse_generated_detail::PartialResult> results;");

            if (vertex_index == automaton.end) {
                writer.line("if (!stack.empty()) {");
                writer.indent();
                writer.line("results.push_back(mparse_generated_detail::PartialResult{");
                writer.indent();
                writer.line(".position = position,");
                writer.line(".stack = stack,");
                writer.unindent();
                writer.line("});");
                writer.unindent();
                writer.line("}");
            }

            const auto& vertex = automaton.vertices.at(vertex_index);
            for (const auto& edge : vertex.edges) {
                emitTransition(writer, automaton, edge, action_ids);
            }

            writer.line("return results;");
            writer.unindent();
            writer.line("}");
            writer.line();
        }

        void emitSymbolCode(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const ReduceActionIds& action_ids
        ) {
            emitParseFunction(writer, automaton);
            for (size_t vertex_index = 0; vertex_index < automaton.vertices.size(); vertex_index++) {
                emitVertexFunction(writer, automaton, vertex_index, action_ids);
            }
        }

        void emitRootParseFunction(
            Writer& writer,
            const analysis::SymbolPtr& root_symbol
        ) {
            writer.line("std::optional<" + symbolValueType(root_symbol) + "> parse(std::string_view input" + parametersDeclaration(root_symbol) + ") {");
            writer.indent();
            writer.line(
                "for (const auto& result : " + parseFunctionName(root_symbol) +
                "(input, 0" + parametersForwarding(root_symbol) + ")) {"
            );
            writer.indent();
            writer.line("if (result.position == input.size()) {");
            writer.indent();
            writer.line("return result.value;");
            writer.unindent();
            writer.line("}");
            writer.unindent();
            writer.line("}");
            writer.line("return std::nullopt;");
            writer.unindent();
            writer.line("}");
        }

    } // namespace

    std::string generateCpp(
        const spec::Specification& specification,
        const analysis::SymbolsStorage& symbols_storage,
        const analysis::GrammarAutomata& automata,
        std::string_view root_symbol_name
    ) {
        const auto root_name = rootSymbolName(specification, root_symbol_name);
        const auto root_symbol = symbols_storage.getSymbolByName(root_name);

        ReduceActionIds action_ids;
        collectReduceActions(automata, action_ids);

        Writer writer;
        writer.line("// This code is automatically generated by mparse. Do not edit.");
        writer.line();
        emitSourceGrammarComment(writer, specification);
        emitRuntime(writer);
        emitForwardDeclarations(writer, automata);
        emitActions(writer, automata, action_ids);

        for (const auto& automaton : automata.symbols()) {
            emitSymbolCode(writer, automaton, action_ids);
        }

        emitRootParseFunction(writer, root_symbol);
        return writer.str();
    }

} // namespace mparse::codegen::cpp
