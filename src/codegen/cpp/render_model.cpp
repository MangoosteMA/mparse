#include "render_model.h"

#include <codegen/cpp/action_emitter.h>
#include <codegen/cpp/identifiers.h>
#include <codegen/cpp/renderer.h>
#include <codegen/cpp/templates.h>
#include <codegen/cpp/writer.h>
#include <mparse/utils.h>

#include <nlohmann/json.hpp>

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

        struct ActionCodegenData {
            ReduceActionIds reduce_action_ids;
            ActionNodeIds action_node_ids;
            ActionFunctionSignatures action_signatures;
            std::vector<const analysis::ActionTreeNode*> action_nodes;
        };

        std::string symbolCppValueType(const analysis::SymbolPtr& symbol) {
            return VERIFY(symbol)->type().value_or("std::monostate");
        }

        std::string semanticValueCppType(
            const analysis::SemanticValueType& semantic_value_type
        ) {
            return std::visit(
                mparse::Overloaded{
                    [](const analysis::TextSemanticValue&) {
                        return std::string{"std::string"};
                    },
                    [](const analysis::CharacterSemanticValue&) {
                        return std::string{"char"};
                    },
                    [](const analysis::SymbolSemanticValue& value_type) {
                        return symbolCppValueType(value_type.symbol);
                    },
                },
                semantic_value_type
            );
        }

        std::vector<std::string> semanticValueCppTypes(
            const std::vector<analysis::SemanticValueType>& semantic_value_types
        ) {
            std::vector<std::string> result;
            result.reserve(semantic_value_types.size());

            for (const auto& semantic_value_type : semantic_value_types) {
                result.push_back(semanticValueCppType(semantic_value_type));
            }

            return result;
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

        std::string parameterFields(
            const analysis::SymbolPtr& symbol,
            std::string_view indentation
        ) {
            std::string result;
            for (const auto& argument : VERIFY(symbol)->arguments()) {
                result += indentation;
                result += argument.type + " " + argument.name + ";\n";
            }
            return result;
        }

        std::string parameterInitializers(
            const analysis::SymbolPtr& symbol,
            std::string_view indentation
        ) {
            std::string result;
            for (const auto& argument : VERIFY(symbol)->arguments()) {
                result += indentation;
                result += ", " + argument.name + "(" + argument.name + ")\n";
            }
            return result;
        }

        std::string makeVertexGenerator(
            const analysis::SymbolPtr& symbol,
            size_t vertex_index,
            std::string_view input,
            std::string_view position,
            std::string_view stack
        ) {
            return "std::make_unique<" + vertexGeneratorName(symbol, vertex_index) +
                   ">(" + std::string{input} + ", " + std::string{position} +
                   ", " + std::string{stack} + parametersForwarding(symbol) + ")";
        }

        size_t collectActionNode(
            const analysis::ActionTreeNodePtr& action,
            ActionCodegenData& action_data
        ) {
            const auto* node = VERIFY(action).get();
            if (const auto iterator = action_data.action_node_ids.find(node);
                iterator != action_data.action_node_ids.end()) {
                return iterator->second;
            }

            const auto action_id = action_data.action_nodes.size();
            action_data.action_node_ids.emplace(node, action_id);
            action_data.action_nodes.push_back(node);

            for (const auto& edge : action->edges()) {
                collectActionNode(edge.node, action_data);
            }

            return action_id;
        }

        std::vector<std::string> sliceTypes(
            const std::vector<std::string>& types,
            size_t begin,
            size_t end
        ) {
            VERIFY(begin <= end);
            VERIFY(end <= types.size());
            return std::vector<std::string>(
                types.begin() + static_cast<std::ptrdiff_t>(begin),
                types.begin() + static_cast<std::ptrdiff_t>(end)
            );
        }

        std::string actionEdgeResultType(
            const analysis::ActionTreeEdge& edge,
            const std::vector<std::string>& input_types,
            const analysis::GrammarAutomata& automata
        ) {
            if (edge.expanded_symbol_name) {
                return symbolCppValueType(
                    automata.getAutomatonByName(*edge.expanded_symbol_name).symbol
                );
            }

            if (edge.offset_right == edge.offset_left + 1) {
                return input_types.at(edge.offset_left);
            }

            VERIFY(false);
            return {};
        }

        void registerActionSignature(
            const analysis::ActionTreeNodePtr& action,
            std::string result_type,
            std::vector<std::string> input_types,
            const analysis::SymbolPtr& parameter_scope,
            const analysis::GrammarAutomata& automata,
            ActionCodegenData& action_data
        ) {
            const auto* node = VERIFY(action).get();
            const auto [iterator, inserted] = action_data.action_signatures.emplace(
                node,
                ActionFunctionSignature{
                    .result_type = result_type,
                    .input_types = input_types,
                    .parameters_declaration = parametersDeclaration(parameter_scope),
                    .parameters_forwarding = parametersForwarding(parameter_scope),
                }
            );

            if (!inserted) {
                VERIFY(iterator->second.result_type == result_type);
                VERIFY(iterator->second.input_types == input_types);
                VERIFY(iterator->second.parameters_declaration == parametersDeclaration(parameter_scope));
                VERIFY(iterator->second.parameters_forwarding == parametersForwarding(parameter_scope));
                return;
            }

            VERIFY(input_types.size() == action->size());
            for (const auto& edge : action->edges()) {
                const auto child_input_types =
                    sliceTypes(input_types, edge.offset_left, edge.offset_right);
                registerActionSignature(
                    edge.node,
                    actionEdgeResultType(edge, input_types, automata),
                    child_input_types,
                    parameter_scope,
                    automata,
                    action_data
                );
            }
        }

        ActionCodegenData collectActionCodegenData(
            const analysis::GrammarAutomata& automata
        ) {
            ActionCodegenData action_data;

            for (const auto& automaton : automata.symbols()) {
                for (const auto& vertex : automaton.vertices) {
                    for (const auto& edge : vertex.edges) {
                        const auto* reduce =
                            std::get_if<analysis::ReduceTransition>(&edge.transition);
                        if (!reduce) {
                            continue;
                        }

                        const auto action_id =
                            collectActionNode(reduce->action, action_data);
                        action_data.reduce_action_ids.emplace(reduce, action_id);
                        registerActionSignature(
                            reduce->action,
                            semanticValueCppType(reduce->result_type),
                            semanticValueCppTypes(reduce->argument_types),
                            automaton.symbol,
                            automata,
                            action_data
                        );
                    }
                }
            }

            return action_data;
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

        enum class RegexFormatPrecedence {
            Alternative,
            Sequence,
            Repeat,
            Atom,
        };

        std::string maybeParenthesizeRegex(
            std::string expression,
            RegexFormatPrecedence expression_precedence,
            RegexFormatPrecedence parent_precedence
        ) {
            if (expression_precedence < parent_precedence) {
                return "(" + expression + ")";
            }
            return expression;
        }

        std::string formatRegexExpression(
            const spec::RegexExpression& expression,
            RegexFormatPrecedence parent_precedence = RegexFormatPrecedence::Alternative
        );

        std::string formatRegexExpressionPtr(
            const spec::RegexExpressionPtr& expression,
            RegexFormatPrecedence parent_precedence
        ) {
            return formatRegexExpression(*VERIFY(expression), parent_precedence);
        }

        std::string formatRegexExpression(
            const spec::RegexExpression& expression,
            RegexFormatPrecedence parent_precedence
        ) {
            return std::visit(
                mparse::Overloaded{
                    [&](const spec::RegexLiteral& literal) {
                        return "'" + escapeGrammarLiteral(literal.value) + "'";
                    },
                    [&](const spec::RegexRange& range) {
                        return "'" + escapeGrammarLiteral(std::string{range.from}) +
                               "'-'" + escapeGrammarLiteral(std::string{range.to}) + "'";
                    },
                    [&](const spec::RegexSequence& sequence) {
                        std::vector<std::string> items;
                        items.reserve(sequence.items.size());
                        for (const auto& item : sequence.items) {
                            items.push_back(formatRegexExpressionPtr(
                                item,
                                RegexFormatPrecedence::Sequence
                            ));
                        }
                        return maybeParenthesizeRegex(
                            join(items, ""),
                            RegexFormatPrecedence::Sequence,
                            parent_precedence
                        );
                    },
                    [&](const spec::RegexAlternative& alternative) {
                        std::vector<std::string> alternatives;
                        alternatives.reserve(alternative.alternatives.size());
                        for (const auto& item : alternative.alternatives) {
                            alternatives.push_back(formatRegexExpressionPtr(
                                item,
                                RegexFormatPrecedence::Alternative
                            ));
                        }
                        return maybeParenthesizeRegex(
                            join(alternatives, " | "),
                            RegexFormatPrecedence::Alternative,
                            parent_precedence
                        );
                    },
                    [&](const spec::RegexRepeat& repeat) {
                        const auto suffix =
                            repeat.kind == spec::RegexRepeatKind::ZeroOrMore ? "*" : "+";
                        return maybeParenthesizeRegex(
                            formatRegexExpressionPtr(
                                repeat.item,
                                RegexFormatPrecedence::Repeat
                            ) + suffix,
                            RegexFormatPrecedence::Repeat,
                            parent_precedence
                        );
                    },
                },
                expression.value
            );
        }

        struct RegexMatcherCode {
            std::string functions;
            std::string root_matcher;
        };

        class RegexMatcherEmitter {
        public:
            RegexMatcherCode emit(const spec::RegexExpression& expression) {
                const auto root_matcher = emitExpression(expression);

                Writer writer;
                for (const auto& function : functions_) {
                    writer.write(function);
                }

                return RegexMatcherCode{
                    .functions = writer.str(),
                    .root_matcher = root_matcher,
                };
            }

        private:
            std::string emitExpressionPtr(const spec::RegexExpressionPtr& expression) {
                return emitExpression(*VERIFY(expression));
            }

            std::string emitExpression(const spec::RegexExpression& expression) {
                if (const auto iterator = matcher_names_.find(&expression);
                    iterator != matcher_names_.end()) {
                    return iterator->second;
                }

                const auto name = "regex_match_" + std::to_string(next_matcher_id_++);
                matcher_names_.emplace(&expression, name);

                functions_.push_back(std::visit(
                    mparse::Overloaded{
                        [&](const spec::RegexLiteral& literal) {
                            return emitLiteralMatcher(name, literal);
                        },
                        [&](const spec::RegexRange& range) {
                            return emitRangeMatcher(name, range);
                        },
                        [&](const spec::RegexSequence& sequence) {
                            return emitSequenceMatcher(name, sequence);
                        },
                        [&](const spec::RegexAlternative& alternative) {
                            return emitAlternativeMatcher(name, alternative);
                        },
                        [&](const spec::RegexRepeat& repeat) {
                            return emitRepeatMatcher(name, repeat);
                        },
                    },
                    expression.value
                ));

                return name;
            }

            std::string emitLiteralMatcher(
                const std::string& name,
                const spec::RegexLiteral& literal
            ) {
                Writer writer;
                writer.line("            auto " + name + " = [&](auto&& self, size_t position) -> std::vector<size_t> {");
                writer.line("                static_cast<void>(self);");
                writer.line("                const std::string_view literal = \"" + escapeStringLiteral(literal.value) + "\";");
                writer.line("                if (position > input_.size() || input_.substr(position, literal.size()) != literal) {");
                writer.line("                    return {};");
                writer.line("                }");
                writer.line("                return {position + literal.size()};");
                writer.line("            };");
                return writer.str();
            }

            std::string emitRangeMatcher(
                const std::string& name,
                const spec::RegexRange& range
            ) {
                Writer writer;
                writer.line("            auto " + name + " = [&](auto&& self, size_t position) -> std::vector<size_t> {");
                writer.line("                static_cast<void>(self);");
                writer.line(
                    "                if (!(position < input_.size() && input_[position] >= '" +
                    escapeCharLiteral(range.from) + "' && input_[position] <= '" +
                    escapeCharLiteral(range.to) + "')) {"
                );
                writer.line("                    return {};");
                writer.line("                }");
                writer.line("                return {position + 1};");
                writer.line("            };");
                return writer.str();
            }

            std::string emitSequenceMatcher(
                const std::string& name,
                const spec::RegexSequence& sequence
            ) {
                std::vector<std::string> item_matchers;
                item_matchers.reserve(sequence.items.size());
                for (const auto& item : sequence.items) {
                    item_matchers.push_back(emitExpressionPtr(item));
                }

                Writer writer;
                writer.line("            auto " + name + " = [&](auto&& self, size_t position) -> std::vector<size_t> {");
                writer.line("                static_cast<void>(self);");
                writer.line("                std::vector<size_t> current_positions{position};");
                for (const auto& item_matcher : item_matchers) {
                    writer.line("                {");
                    writer.line("                    std::vector<size_t> next_positions;");
                    writer.line("                    for (const auto current_position : current_positions) {");
                    writer.line(
                        "                        for (const auto matched_position : " +
                        item_matcher + "(" + item_matcher + ", current_position)) {"
                    );
                    writer.line("                            mparse_generated_detail::pushUnique(next_positions, matched_position);");
                    writer.line("                        }");
                    writer.line("                    }");
                    writer.line("                    current_positions = std::move(next_positions);");
                    writer.line("                    if (current_positions.empty()) {");
                    writer.line("                        return {};");
                    writer.line("                    }");
                    writer.line("                }");
                }
                writer.line("                return current_positions;");
                writer.line("            };");
                return writer.str();
            }

            std::string emitAlternativeMatcher(
                const std::string& name,
                const spec::RegexAlternative& alternative
            ) {
                std::vector<std::string> alternative_matchers;
                alternative_matchers.reserve(alternative.alternatives.size());
                for (const auto& item : alternative.alternatives) {
                    alternative_matchers.push_back(emitExpressionPtr(item));
                }

                Writer writer;
                writer.line("            auto " + name + " = [&](auto&& self, size_t position) -> std::vector<size_t> {");
                writer.line("                static_cast<void>(self);");
                writer.line("                std::vector<size_t> result;");
                for (const auto& alternative_matcher : alternative_matchers) {
                    writer.line(
                        "                for (const auto matched_position : " +
                        alternative_matcher + "(" + alternative_matcher + ", position)) {"
                    );
                    writer.line("                    mparse_generated_detail::pushUnique(result, matched_position);");
                    writer.line("                }");
                }
                writer.line("                return result;");
                writer.line("            };");
                return writer.str();
            }

            std::string emitRepeatMatcher(
                const std::string& name,
                const spec::RegexRepeat& repeat
            ) {
                const auto item_matcher = emitExpressionPtr(repeat.item);

                Writer writer;
                writer.line("            auto " + name + " = [&](auto&& self, size_t position) -> std::vector<size_t> {");
                writer.line("                std::vector<size_t> result;");
                if (repeat.kind == spec::RegexRepeatKind::ZeroOrMore) {
                    writer.line("                mparse_generated_detail::pushUnique(result, position);");
                }
                writer.line(
                    "                for (const auto matched_position : " +
                    item_matcher + "(" + item_matcher + ", position)) {"
                );
                writer.line("                    mparse_generated_detail::pushUnique(result, matched_position);");
                writer.line("                    if (matched_position == position) {");
                writer.line("                        continue;");
                writer.line("                    }");
                writer.line("                    for (const auto repeated_position : self(self, matched_position)) {");
                writer.line("                        mparse_generated_detail::pushUnique(result, repeated_position);");
                writer.line("                    }");
                writer.line("                }");
                writer.line("                return result;");
                writer.line("            };");
                return writer.str();
            }

            std::unordered_map<const spec::RegexExpression*, std::string> matcher_names_;
            std::vector<std::string> functions_;
            size_t next_matcher_id_ = 0;
        };

        RegexMatcherCode emitRegexMatcherCode(const spec::RegexExpression& expression) {
            RegexMatcherEmitter emitter;
            return emitter.emit(expression);
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
                    [](const spec::RuleItemRepeatedLiteral& literal) {
                        return "'" + escapeGrammarLiteral(literal.value) + "'^" +
                               literal.count_expression;
                    },
                    [](const spec::RuleItemRegex& regex) {
                        return formatRegexExpression(regex.expression);
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
            writer.write(runtimeTemplate());
        }

        void emitForwardDeclarations(
            Writer& writer,
            const analysis::GrammarAutomata& automata
        ) {
            for (const auto& automaton : automata.symbols()) {
                writer.line("class " + parseGeneratorName(automaton.symbol) + ";");
                for (size_t vertex_index = 0; vertex_index < automaton.vertices.size(); vertex_index++) {
                    writer.line("class " + vertexGeneratorName(automaton.symbol, vertex_index) + ";");
                }
            }
            writer.line();
        }

        void emitActions(
            Writer& writer,
            const ActionCodegenData& action_data
        ) {
            for (size_t action_index = 0; action_index < action_data.action_nodes.size(); action_index++) {
                emitActionFunctionForwardDeclaration(
                    writer,
                    action_index,
                    action_data.action_signatures.at(action_data.action_nodes.at(action_index))
                );
            }
            writer.line();

            for (size_t action_index = 0; action_index < action_data.action_nodes.size(); action_index++) {
                emitActionFunction(
                    writer,
                    action_index,
                    *VERIFY(action_data.action_nodes.at(action_index)),
                    action_data.action_node_ids,
                    action_data.action_signatures
                );
                writer.line();
            }
        }

        void emitParseGeneratorDeclaration(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton
        ) {
            const auto value_type = symbolCppValueType(automaton.symbol);
            const auto generator_name = parseGeneratorName(automaton.symbol);

            writer.write(renderTemplate(
                parseGeneratorDeclarationTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"value_type", value_type},
                    {"parameters_declaration", parametersDeclaration(automaton.symbol)},
                }
            ));
        }

        void emitVertexGeneratorDeclaration(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index
        ) {
            const auto generator_name = vertexGeneratorName(automaton.symbol, vertex_index);

            writer.write(renderTemplate(
                vertexGeneratorDeclarationTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"parameters_declaration", parametersDeclaration(automaton.symbol)},
                    {"parameter_fields", parameterFields(automaton.symbol, "    ")},
                }
            ));
        }

        void emitParseGeneratorDefinition(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton
        ) {
            const auto generator_name = parseGeneratorName(automaton.symbol);
            const auto value_type = symbolCppValueType(automaton.symbol);

            writer.write(renderTemplate(
                parseGeneratorDefinitionTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"parameters_declaration", parametersDeclaration(automaton.symbol)},
                    {"start_generator", makeVertexGenerator(automaton.symbol, automaton.start, "input", "position", "std::vector<std::any>{}")},
                    {"value_type", value_type},
                }
            ));
        }

        void emitVertexGeneratorConstructor(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index
        ) {
            const auto generator_name = vertexGeneratorName(automaton.symbol, vertex_index);

            writer.write(renderTemplate(
                vertexGeneratorConstructorTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"parameters_declaration", parametersDeclaration(automaton.symbol)},
                    {"parameter_initializers", parameterInitializers(automaton.symbol, "    ")},
                    {"terminal_pending", vertex_index == automaton.end ? "!stack_.empty()" : "false"},
                }
            ));
        }

        void emitVertexGeneratorMakeNext(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index
        ) {
            const auto generator_name = vertexGeneratorName(automaton.symbol, vertex_index);
            const auto& vertex = automaton.vertices.at(vertex_index);

            writer.write(renderTemplate(
                vertexGeneratorMakeNextTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"has_terminal", vertex_index == automaton.end},
                    {"has_edges", !vertex.edges.empty()},
                    {"edge_count", vertex.edges.size()},
                }
            ));
        }

        void emitLiteralEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::LiteralTransition& literal
        ) {
            writer.write(renderTemplate(
                literalEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"literal", escapeStringLiteral(literal.value)},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input_", "position_ + literal.size()", "std::move(next_stack)")},
                }
            ));
        }

        void emitRangeEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::RangeTransition& range
        ) {
            writer.write(renderTemplate(
                rangeEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"from", escapeCharLiteral(range.from)},
                    {"to", escapeCharLiteral(range.to)},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input_", "position_ + 1", "std::move(next_stack)")},
                }
            ));
        }

        void emitRepeatedLiteralEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::RepeatedLiteralTransition& literal
        ) {
            writer.write(renderTemplate(
                repeatedLiteralEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"literal", escapeStringLiteral(literal.value)},
                    {"count_expression", literal.count_expression},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input_", "position_ + repeated_size", "std::move(next_stack)")},
                }
            ));
        }

        void emitRegexEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::RegexTransition& regex
        ) {
            const auto matcher_code = emitRegexMatcherCode(regex.expression);
            writer.write(renderTemplate(
                regexEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"matcher_functions", matcher_code.functions},
                    {"root_matcher", matcher_code.root_matcher},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input", "match_position", "std::move(next_stack)")},
                }
            ));
        }

        void emitSymbolEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::SymbolTransition& symbol_transition
        ) {
            std::string arguments;
            for (const auto& argument : symbol_transition.arguments) {
                arguments += ", " + argument;
            }

            writer.write(renderTemplate(
                symbolEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"parameters_declaration", parametersDeclaration(automaton.symbol)},
                    {"parameter_initializers", parameterInitializers(automaton.symbol, "                    ")},
                    {"nested_arguments", arguments},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input", "nested->position", "std::move(next_stack)")},
                    {"parameter_fields", parameterFields(automaton.symbol, "                ")},
                    {"nested_generator_name", parseGeneratorName(symbol_transition.symbol)},
                    {"parameters_forwarding", parametersForwarding(automaton.symbol)},
                }
            ));
        }

        void emitReduceEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const analysis::ReduceTransition& reduce,
            const ReduceActionIds& reduce_action_ids
        ) {
            writer.write(renderTemplate(
                reduceEdgeGeneratorCaseTemplate(),
                nlohmann::json{
                    {"edge_index", edge_index},
                    {"left_brace", "{"},
                    {"action_name", actionFunctionName(reduce_action_ids.at(&reduce))},
                    {"parameters_forwarding", parametersForwarding(automaton.symbol)},
                    {"right_brace", "}"},
                    {"next_generator", makeVertexGenerator(automaton.symbol, edge.target, "input_", "position_", "std::move(next_stack)")},
                }
            ));
        }

        void emitEdgeGeneratorCase(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            const analysis::AutomatonEdge& edge,
            size_t edge_index,
            const ReduceActionIds& reduce_action_ids
        ) {
            std::visit(
                mparse::Overloaded{
                    [&](const analysis::LiteralTransition& literal) {
                        emitLiteralEdgeGeneratorCase(writer, automaton, edge, edge_index, literal);
                    },
                    [&](const analysis::RangeTransition& range) {
                        emitRangeEdgeGeneratorCase(writer, automaton, edge, edge_index, range);
                    },
                    [&](const analysis::RepeatedLiteralTransition& literal) {
                        emitRepeatedLiteralEdgeGeneratorCase(writer, automaton, edge, edge_index, literal);
                    },
                    [&](const analysis::RegexTransition& regex) {
                        emitRegexEdgeGeneratorCase(writer, automaton, edge, edge_index, regex);
                    },
                    [&](const analysis::SymbolTransition& symbol) {
                        emitSymbolEdgeGeneratorCase(writer, automaton, edge, edge_index, symbol);
                    },
                    [&](const analysis::ReduceTransition& reduce) {
                        emitReduceEdgeGeneratorCase(writer, automaton, edge, edge_index, reduce, reduce_action_ids);
                    },
                },
                edge.transition
            );
        }

        void emitVertexGeneratorMakeEdge(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index,
            const ReduceActionIds& reduce_action_ids
        ) {
            const auto generator_name = vertexGeneratorName(automaton.symbol, vertex_index);
            const auto& vertex = automaton.vertices.at(vertex_index);

            Writer cases_writer;
            for (size_t edge_index = 0; edge_index < vertex.edges.size(); edge_index++) {
                emitEdgeGeneratorCase(cases_writer, automaton, vertex.edges.at(edge_index), edge_index, reduce_action_ids);
            }

            writer.write(renderTemplate(
                vertexGeneratorMakeEdgeTemplate(),
                nlohmann::json{
                    {"generator_name", generator_name},
                    {"cases", cases_writer.str()},
                }
            ));
        }

        void emitVertexGeneratorDefinitions(
            Writer& writer,
            const analysis::SymbolAutomaton& automaton,
            size_t vertex_index,
            const ReduceActionIds& reduce_action_ids
        ) {
            emitVertexGeneratorConstructor(writer, automaton, vertex_index);
            emitVertexGeneratorMakeNext(writer, automaton, vertex_index);
            emitVertexGeneratorMakeEdge(writer, automaton, vertex_index, reduce_action_ids);
        }

        void emitSymbolClassDeclarations(
            Writer& writer,
            const analysis::GrammarAutomata& automata
        ) {
            for (const auto& automaton : automata.symbols()) {
                emitParseGeneratorDeclaration(writer, automaton);
            }

            for (const auto& automaton : automata.symbols()) {
                for (size_t vertex_index = 0; vertex_index < automaton.vertices.size(); vertex_index++) {
                    emitVertexGeneratorDeclaration(writer, automaton, vertex_index);
                }
            }
        }

        void emitSymbolDefinitions(
            Writer& writer,
            const analysis::GrammarAutomata& automata,
            const ReduceActionIds& reduce_action_ids
        ) {
            for (const auto& automaton : automata.symbols()) {
                emitParseGeneratorDefinition(writer, automaton);
            }

            for (const auto& automaton : automata.symbols()) {
                for (size_t vertex_index = 0; vertex_index < automaton.vertices.size(); vertex_index++) {
                    emitVertexGeneratorDefinitions(writer, automaton, vertex_index, reduce_action_ids);
                }
            }
        }

        void emitSymbols(
            Writer& writer,
            const analysis::GrammarAutomata& automata,
            const ReduceActionIds& reduce_action_ids
        ) {
            emitSymbolClassDeclarations(writer, automata);
            emitSymbolDefinitions(writer, automata, reduce_action_ids);
        }

        void emitRootParseFunction(
            Writer& writer,
            const analysis::SymbolPtr& root_symbol
        ) {
            writer.write(renderTemplate(
                rootParseFunctionTemplate(),
                nlohmann::json{
                    {"value_type", symbolCppValueType(root_symbol)},
                    {"parameters_declaration", parametersDeclaration(root_symbol)},
                    {"generator_name", parseGeneratorName(root_symbol)},
                    {"parameters_forwarding", parametersForwarding(root_symbol)},
                }
            ));
        }

        template <typename Emit>
        std::string renderSection(Emit emit) {
            Writer writer;
            emit(writer);
            return writer.str();
        }

    } // namespace

    nlohmann::json buildRenderModel(
        const spec::Specification& specification,
        const analysis::GrammarAutomata& automata,
        const analysis::SymbolPtr& root_symbol
    ) {
        const auto action_data = collectActionCodegenData(automata);

        return nlohmann::json{
            {"banner", "// This code is automatically generated by mparse. Do not edit."},
            {"source_grammar", renderSection([&](Writer& writer) {
                 emitSourceGrammarComment(writer, specification);
             })},
            {"runtime", renderSection(emitRuntime)},
            {"forward_declarations", renderSection([&](Writer& writer) {
                 emitForwardDeclarations(writer, automata);
             })},
            {"actions", renderSection([&](Writer& writer) {
                 emitActions(writer, action_data);
             })},
            {"symbols", renderSection([&](Writer& writer) {
                 emitSymbols(writer, automata, action_data.reduce_action_ids);
             })},
            {"root_parse_function", renderSection([&](Writer& writer) {
                 emitRootParseFunction(writer, root_symbol);
             })},
        };
    }

} // namespace mparse::codegen::cpp
