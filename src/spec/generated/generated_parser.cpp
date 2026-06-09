// mparse spec grammar.
//
// Version: 1.

/*
This file is the canonical grammar for mparse specification files and the
source template for the bootstrap parser snapshot.

The grammar intentionally covers only the current spec syntax: symbols with
optional arguments and value type, literal/range/repeated-literal/symbol rule
items, rule actions, and surrounding source template text. Keep semantic
helpers in this file close to the grammar rules so a new parser snapshot can be
regenerated from one entry point.

To update the snapshot, build mparse and run:

    cmake --build <build-dir> --target regenerate_spec_parser

Do not edit src/spec/generated/generated_parser.cpp by hand.
*/

#include <spec/data.h>
#include <spec/generated/generated_parser.h>
#include <spec/utils.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mparse::spec::generated {

    struct LiteralData {
        std::string value;
        std::string source;
    };

    struct ExpressionData {
        std::string value;
        std::string source;
    };

    struct ParsedRuleItem {
        mparse::spec::RuleItem item;
        std::string source;
    };

    struct RuleTailData {
        std::vector<mparse::spec::RuleItem> items;
        std::optional<std::string> action;
        std::string source;
    };

    struct RuleData {
        mparse::spec::Rule rule;
        std::string source;
    };

    struct RulesData {
        std::vector<mparse::spec::Rule> rules;
        std::string source;
    };

    struct ParsedSymbol {
        mparse::spec::Symbol symbol;
        std::string leading;
        std::string source;
    };

    struct ParsedSymbols {
        std::vector<ParsedSymbol> symbols;
        std::string source;
    };

    struct SourcePosition {
        size_t line = 1;
        size_t column = 1;
    };

    std::string oneChar(char value) {
        return std::string(1, value);
    }

    LiteralData emptyLiteralData() {
        return {};
    }

    LiteralData prependLiteralChar(std::string value, const LiteralData& tail) {
        return LiteralData{
            .value = value + tail.value,
            .source = value + tail.source,
        };
    }

    LiteralData prependEscapedLiteralChar(
        const LiteralData& escaped,
        const LiteralData& tail
    ) {
        return LiteralData{
            .value = escaped.value + tail.value,
            .source = "\\" + escaped.source + tail.source,
        };
    }

    LiteralData makeEscapedLiteralChar(std::string source) {
        if (source == "n") {
            return LiteralData{.value = "\n", .source = std::move(source)};
        }
        if (source == "t") {
            return LiteralData{.value = "\t", .source = std::move(source)};
        }
        return LiteralData{.value = source, .source = std::move(source)};
    }

    LiteralData makeLiteral(const LiteralData& content) {
        return LiteralData{
            .value = content.value,
            .source = "'" + content.source + "'",
        };
    }

    ParsedRuleItem makeLiteralItem(const LiteralData& literal) {
        return ParsedRuleItem{
            .item = mparse::spec::RuleItem{
                .value = mparse::spec::RuleItemLiteral{.value = literal.value},
            },
            .source = literal.source,
        };
    }

    ParsedRuleItem makeRepeatedLiteralItem(
        const LiteralData& literal,
        std::string horizontal_whitespace,
        const ExpressionData& count_expression
    ) {
        return ParsedRuleItem{
            .item = mparse::spec::RuleItem{
                .value = mparse::spec::RuleItemRepeatedLiteral{
                    .value = literal.value,
                    .count_expression = count_expression.value,
                },
            },
            .source = literal.source + horizontal_whitespace + "^" +
                      count_expression.source,
        };
    }

    ParsedRuleItem makeRangeItem(
        const LiteralData& from,
        std::string horizontal_whitespace_left,
        std::string horizontal_whitespace_right,
        const LiteralData& to
    ) {
        if (from.value.size() != 1 || to.value.size() != 1) {
            throw std::runtime_error("range boundaries must be one-character literals");
        }

        return ParsedRuleItem{
            .item = mparse::spec::RuleItem{
                .value = mparse::spec::RuleItemRange{
                    .from = from.value.front(),
                    .to = to.value.front(),
                },
            },
            .source = from.source + horizontal_whitespace_left + "-" +
                      horizontal_whitespace_right + to.source,
        };
    }

    std::vector<std::string> splitCommaSeparatedExpressions(std::string_view content) {
        const auto trimmed_content = mparse::spec::trim(content);
        content = trimmed_content;

        std::vector<std::string> result;
        size_t begin = 0;
        size_t depth = 0;
        bool inside_literal = false;
        bool escaped = false;

        for (size_t index = 0; index < content.size(); index++) {
            const char current = content[index];

            if (inside_literal) {
                if (escaped) {
                    escaped = false;
                } else if (current == '\\') {
                    escaped = true;
                } else if (current == '\'') {
                    inside_literal = false;
                }
                continue;
            }

            if (current == '\'') {
                inside_literal = true;
            } else if (current == '[' || current == '(' || current == '<' || current == '{') {
                depth++;
            } else if (current == ']' || current == ')' || current == '>' || current == '}') {
                if (depth > 0) {
                    depth--;
                }
            } else if (current == ',' && depth == 0) {
                result.push_back(mparse::spec::trim(content.substr(begin, index - begin)));
                begin = index + 1;
            }
        }

        const auto last = mparse::spec::trim(content.substr(begin));
        if (!last.empty()) {
            result.push_back(last);
        }
        return result;
    }

    std::vector<mparse::spec::SymbolArgument> parseSymbolArguments(
        std::string_view content
    ) {
        std::vector<mparse::spec::SymbolArgument> arguments;
        for (const auto& raw_argument : splitCommaSeparatedExpressions(content)) {
            const size_t split = raw_argument.find_last_of(" \t");
            if (split == std::string::npos) {
                throw std::runtime_error(
                    "symbol argument must have name and type: " + raw_argument
                );
            }

            arguments.push_back(mparse::spec::SymbolArgument{
                .name = mparse::spec::trim(std::string_view{raw_argument}.substr(0, split)),
                .type = mparse::spec::trim(std::string_view{raw_argument}.substr(split + 1)),
            });
        }
        return arguments;
    }

    ExpressionData makeExpression(std::string source) {
        return ExpressionData{
            .value = source,
            .source = std::move(source),
        };
    }

    ExpressionData makeParenthesizedExpression(std::string content) {
        return ExpressionData{
            .value = mparse::spec::trim(content),
            .source = "(" + content + ")",
        };
    }

    ParsedRuleItem makeSymbolReference(
        std::string name,
        std::string horizontal_whitespace,
        std::string arguments
    ) {
        return ParsedRuleItem{
            .item = mparse::spec::RuleItem{
                .value = mparse::spec::RuleItemSymbol{
                    .name = name,
                    .arguments = splitCommaSeparatedExpressions(arguments),
                },
            },
            .source = name + horizontal_whitespace + "[" + arguments + "]",
        };
    }

    ParsedRuleItem makeSymbolReference(std::string name) {
        return ParsedRuleItem{
            .item = mparse::spec::RuleItem{
                .value = mparse::spec::RuleItemSymbol{.name = name},
            },
            .source = std::move(name),
        };
    }

    RuleTailData emptyRuleTail() {
        return {};
    }

    RuleTailData makeActionTail(std::string action) {
        return RuleTailData{
            .action = mparse::spec::trim(action),
            .source = "{" + action + "}",
        };
    }

    RuleTailData prependRuleItem(
        const ParsedRuleItem& item,
        const RuleTailData& tail
    ) {
        RuleTailData result = tail;
        result.items.insert(result.items.begin(), item.item);
        result.source = item.source + tail.source;
        return result;
    }

    RuleTailData prependHorizontalSpaces(
        std::string spaces,
        const RuleTailData& tail
    ) {
        RuleTailData result = tail;
        result.source = spaces + tail.source;
        return result;
    }

    RuleTailData makeRuleBody(const ParsedRuleItem& item, const RuleTailData& tail) {
        return prependRuleItem(item, tail);
    }

    RuleTailData makeRuleBodyFromAction(std::string action) {
        return makeActionTail(std::move(action));
    }

    RuleData makeRule(std::string prefix, const RuleTailData& body) {
        return RuleData{
            .rule = mparse::spec::Rule{
                .items = body.items,
                .action = body.action,
            },
            .source = std::move(prefix) + body.source,
        };
    }

    RulesData makeRules(const RuleData& rule) {
        return RulesData{
            .rules = {rule.rule},
            .source = rule.source,
        };
    }

    RulesData appendRule(const RulesData& rules, std::string whitespace, const RuleData& rule) {
        RulesData result = rules;
        result.rules.push_back(rule.rule);
        result.source += whitespace + rule.source;
        return result;
    }

    ParsedSymbol makeSymbol(
        std::string name,
        std::string horizontal_whitespace,
        std::string whitespace,
        const RulesData& rules
    ) {
        return ParsedSymbol{
            .symbol = mparse::spec::Symbol{
                .name = name,
                .rules = rules.rules,
            },
            .source = name + horizontal_whitespace + whitespace + rules.source,
        };
    }

    ParsedSymbol makeSymbolWithArguments(
        std::string name,
        std::string horizontal_whitespace_before_arguments,
        std::string arguments,
        std::string horizontal_whitespace_after_arguments,
        std::string whitespace,
        const RulesData& rules
    ) {
        return ParsedSymbol{
            .symbol = mparse::spec::Symbol{
                .name = name,
                .arguments = parseSymbolArguments(arguments),
                .rules = rules.rules,
            },
            .source = name + horizontal_whitespace_before_arguments + "[" + arguments + "]" +
                      horizontal_whitespace_after_arguments + whitespace + rules.source,
        };
    }

    ParsedSymbol makeSymbolWithType(
        std::string name,
        std::string horizontal_whitespace_before_type,
        std::string type,
        std::string horizontal_whitespace_after_type,
        std::string whitespace,
        const RulesData& rules
    ) {
        return ParsedSymbol{
            .symbol = mparse::spec::Symbol{
                .name = name,
                .type = mparse::spec::trim(type),
                .rules = rules.rules,
            },
            .source = name + horizontal_whitespace_before_type + "<" + type + ">" +
                      horizontal_whitespace_after_type + whitespace + rules.source,
        };
    }

    ParsedSymbol makeSymbolWithArgumentsAndType(
        std::string name,
        std::string horizontal_whitespace_before_arguments,
        std::string arguments,
        std::string horizontal_whitespace_before_type,
        std::string type,
        std::string horizontal_whitespace_after_type,
        std::string whitespace,
        const RulesData& rules
    ) {
        return ParsedSymbol{
            .symbol = mparse::spec::Symbol{
                .name = name,
                .type = mparse::spec::trim(type),
                .arguments = parseSymbolArguments(arguments),
                .rules = rules.rules,
            },
            .source = name + horizontal_whitespace_before_arguments + "[" + arguments + "]" +
                      horizontal_whitespace_before_type + "<" + type + ">" +
                      horizontal_whitespace_after_type + whitespace + rules.source,
        };
    }

    ParsedSymbols makeSymbols(const ParsedSymbol& symbol) {
        return ParsedSymbols{
            .symbols = {symbol},
            .source = symbol.source,
        };
    }

    ParsedSymbols appendSymbol(
        const ParsedSymbols& symbols,
        std::string whitespace,
        const ParsedSymbol& symbol
    ) {
        ParsedSymbols result = symbols;
        auto next_symbol = symbol;
        next_symbol.leading = whitespace;
        result.symbols.push_back(std::move(next_symbol));
        result.source += whitespace + symbol.source;
        return result;
    }

    void advanceSourcePosition(SourcePosition& position, std::string_view source) {
        for (const char current : source) {
            if (current == '\n') {
                position.line++;
                position.column = 1;
            } else {
                position.column++;
            }
        }
    }

    std::vector<mparse::spec::Symbol> finalizeSymbols(
        std::string leading_whitespace,
        const ParsedSymbols& symbols
    ) {
        SourcePosition position;
        advanceSourcePosition(position, leading_whitespace);

        std::vector<mparse::spec::Symbol> result;
        result.reserve(symbols.symbols.size());

        for (const auto& parsed_symbol : symbols.symbols) {
            advanceSourcePosition(position, parsed_symbol.leading);
            auto symbol = parsed_symbol.symbol;
            symbol.source_reference = mparse::spec::SourceReference{
                .line = position.line,
                .column = position.column,
            };
            result.push_back(std::move(symbol));
            advanceSourcePosition(position, parsed_symbol.source);
        }

        return result;
    }

} // namespace mparse::spec::generated

namespace mparse_spec_generated = mparse::spec::generated;

// mparse: begin grammar
// This code is automatically generated by mparse. Do not edit.

// Source grammar:
//
// Spec<std::vector<mparse::spec::Symbol>>
//     : Whitespace Symbols Whitespace { $$ = mparse_spec_generated::finalizeSymbols($1, $2) }
//
// Symbols<mparse_spec_generated::ParsedSymbols>
//     : Symbol { $$ = mparse_spec_generated::makeSymbols($1) }
//     : Symbols Whitespace Symbol { $$ = mparse_spec_generated::appendSymbol($1, $2, $3) }
//
// Symbol<mparse_spec_generated::ParsedSymbol>
//     : Identifier HorizontalWhitespace Whitespace Rules { $$ = mparse_spec_generated::makeSymbol($1, $2, $3, $4) }
//     : Identifier HorizontalWhitespace '[' SquareContent ']' HorizontalWhitespace Whitespace Rules { $$ = mparse_spec_generated::makeSymbolWithArguments($1, $2, $4, $6, $7, $8) }
//     : Identifier HorizontalWhitespace '<' AngleContent '>' HorizontalWhitespace Whitespace Rules { $$ = mparse_spec_generated::makeSymbolWithType($1, $2, $4, $6, $7, $8) }
//     : Identifier HorizontalWhitespace '[' SquareContent ']' HorizontalWhitespace '<' AngleContent '>' HorizontalWhitespace Whitespace Rules { $$ = mparse_spec_generated::makeSymbolWithArgumentsAndType($1, $2, $4, $6, $8, $10, $11, $12) }
//
// Rules<mparse_spec_generated::RulesData>
//     : Rule { $$ = mparse_spec_generated::makeRules($1) }
//     : Rules Whitespace Rule { $$ = mparse_spec_generated::appendRule($1, $2, $3) }
//
// Rule<mparse_spec_generated::RuleData>
//     : ':' HorizontalWhitespace RuleBody { $$ = mparse_spec_generated::makeRule($1 + $2, $3) }
//
// RuleBody<mparse_spec_generated::RuleTailData>
//     : { $$ = mparse_spec_generated::emptyRuleTail() }
//     : '{' BraceContent '}' { $$ = mparse_spec_generated::makeRuleBodyFromAction($2) }
//     : RuleItem RuleTail { $$ = mparse_spec_generated::makeRuleBody($1, $2) }
//
// RuleTail<mparse_spec_generated::RuleTailData>
//     : { $$ = mparse_spec_generated::emptyRuleTail() }
//     : '{' BraceContent '}' { $$ = mparse_spec_generated::makeActionTail($2) }
//     : HorizontalSpaces RuleTailAfterSpaces { $$ = mparse_spec_generated::prependHorizontalSpaces($1, $2) }
//
// RuleTailAfterSpaces<mparse_spec_generated::RuleTailData>
//     : RuleItem RuleTail { $$ = mparse_spec_generated::prependRuleItem($1, $2) }
//     : '{' BraceContent '}' { $$ = mparse_spec_generated::makeActionTail($2) }
//
// HorizontalSpaces<std::string>
//     : HorizontalSpace { $$ = $1 }
//     : HorizontalSpace HorizontalSpaces { $$ = $1 + $2 }
//
// RuleItem<mparse_spec_generated::ParsedRuleItem>
//     : LiteralOrRange { $$ = $1 }
//     : SymbolReference { $$ = $1 }
//
// LiteralOrRange<mparse_spec_generated::ParsedRuleItem>
//     : Literal HorizontalWhitespace '-' HorizontalWhitespace Literal { $$ = mparse_spec_generated::makeRangeItem($1, $2, $4, $5) }
//     : Literal HorizontalWhitespace '^' RepeatCountExpression { $$ = mparse_spec_generated::makeRepeatedLiteralItem($1, $2, $4) }
//     : Literal { $$ = mparse_spec_generated::makeLiteralItem($1) }
//
// RepeatCountExpression<mparse_spec_generated::ExpressionData>
//     : Identifier { $$ = mparse_spec_generated::makeExpression($1) }
//     : DecimalNumber { $$ = mparse_spec_generated::makeExpression($1) }
//     : '(' ParenContent ')' { $$ = mparse_spec_generated::makeParenthesizedExpression($2) }
//
// DecimalNumber<std::string>
//     : '0'-'9' { $$ = mparse_spec_generated::oneChar($1) }
//     : DecimalNumber '0'-'9' { $$ = $1 + mparse_spec_generated::oneChar($2) }
//
// SymbolReference<mparse_spec_generated::ParsedRuleItem>
//     : Identifier { $$ = mparse_spec_generated::makeSymbolReference($1) }
//     : Identifier HorizontalWhitespace '[' SquareContent ']' { $$ = mparse_spec_generated::makeSymbolReference($1, $2, $4) }
//
// Identifier<std::string>
//     : IdentifierStart { $$ = $1 }
//     : Identifier IdentifierPart { $$ = $1 + $2 }
//
// IdentifierStart<std::string>
//     : 'a'-'z' { $$ = mparse_spec_generated::oneChar($1) }
//     : 'A'-'Z' { $$ = mparse_spec_generated::oneChar($1) }
//     : '_' { $$ = $1 }
//
// IdentifierPart<std::string>
//     : IdentifierStart { $$ = $1 }
//     : '0'-'9' { $$ = mparse_spec_generated::oneChar($1) }
//
// Whitespace<std::string>
//     : { $$ = "" }
//     : WhitespaceChar Whitespace { $$ = $1 + $2 }
//
// HorizontalWhitespace<std::string>
//     : { $$ = "" }
//     : HorizontalSpace HorizontalWhitespace { $$ = $1 + $2 }
//
// WhitespaceChar<std::string>
//     : HorizontalSpace { $$ = $1 }
//     : LineBreak { $$ = $1 }
//
// HorizontalSpace<std::string>
//     : ' ' { $$ = $1 }
//     : '\t' { $$ = $1 }
//
// LineBreak<std::string>
//     : '\n' { $$ = $1 }
//
// BraceContent<std::string>
//     : { $$ = "" }
//     : BracePlainChar BraceContent { $$ = $1 + $2 }
//     : Literal BraceContent { $$ = $1.source + $2 }
//     : '{' BraceContent '}' BraceContent { $$ = $1 + $2 + $3 + $4 }
//
// BracePlainChar<std::string>
//     : LineBreak { $$ = $1 }
//     : HorizontalSpace { $$ = $1 }
//     : '!'-'&' { $$ = mparse_spec_generated::oneChar($1) }
//     : '('-'z' { $$ = mparse_spec_generated::oneChar($1) }
//     : '|' { $$ = $1 }
//     : '~' { $$ = $1 }
//
// SquareContent<std::string>
//     : { $$ = "" }
//     : SquarePlainChar SquareContent { $$ = $1 + $2 }
//     : Literal SquareContent { $$ = $1.source + $2 }
//     : '[' SquareContent ']' SquareContent { $$ = $1 + $2 + $3 + $4 }
//
// SquarePlainChar<std::string>
//     : LineBreak { $$ = $1 }
//     : HorizontalSpace { $$ = $1 }
//     : '!'-'&' { $$ = mparse_spec_generated::oneChar($1) }
//     : '('-'Z' { $$ = mparse_spec_generated::oneChar($1) }
//     : Backslash { $$ = $1 }
//     : '^'-'~' { $$ = mparse_spec_generated::oneChar($1) }
//
// AngleContent<std::string>
//     : { $$ = "" }
//     : AnglePlainChar AngleContent { $$ = $1 + $2 }
//     : Literal AngleContent { $$ = $1.source + $2 }
//     : '<' AngleContent '>' AngleContent { $$ = $1 + $2 + $3 + $4 }
//
// AnglePlainChar<std::string>
//     : LineBreak { $$ = $1 }
//     : HorizontalSpace { $$ = $1 }
//     : '!'-'&' { $$ = mparse_spec_generated::oneChar($1) }
//     : '('-';' { $$ = mparse_spec_generated::oneChar($1) }
//     : '=' { $$ = $1 }
//     : '?'-'~' { $$ = mparse_spec_generated::oneChar($1) }
//
// ParenContent<std::string>
//     : { $$ = "" }
//     : ParenPlainChar ParenContent { $$ = $1 + $2 }
//     : Literal ParenContent { $$ = $1.source + $2 }
//     : '(' ParenContent ')' ParenContent { $$ = $1 + $2 + $3 + $4 }
//
// ParenPlainChar<std::string>
//     : LineBreak { $$ = $1 }
//     : HorizontalSpace { $$ = $1 }
//     : '!'-'&' { $$ = mparse_spec_generated::oneChar($1) }
//     : '*'-'~' { $$ = mparse_spec_generated::oneChar($1) }
//
// Literal<mparse_spec_generated::LiteralData>
//     : Quote LiteralContent Quote { $$ = mparse_spec_generated::makeLiteral($2) }
//
// LiteralContent<mparse_spec_generated::LiteralData>
//     : { $$ = mparse_spec_generated::emptyLiteralData() }
//     : LiteralChar LiteralContent { $$ = mparse_spec_generated::prependLiteralChar($1, $2) }
//     : Backslash EscapedLiteralChar LiteralContent { $$ = mparse_spec_generated::prependEscapedLiteralChar($2, $3) }
//
// LiteralChar<std::string>
//     : LineBreak { $$ = $1 }
//     : HorizontalSpace { $$ = $1 }
//     : '!'-'&' { $$ = mparse_spec_generated::oneChar($1) }
//     : '('-'[' { $$ = mparse_spec_generated::oneChar($1) }
//     : ']'-'~' { $$ = mparse_spec_generated::oneChar($1) }
//
// EscapedLiteralChar<mparse_spec_generated::LiteralData>
//     : LiteralChar { $$ = mparse_spec_generated::makeEscapedLiteralChar($1) }
//     : Quote { $$ = mparse_spec_generated::makeEscapedLiteralChar($1) }
//     : Backslash { $$ = mparse_spec_generated::makeEscapedLiteralChar($1) }
//     : 'n' { $$ = mparse_spec_generated::makeEscapedLiteralChar($1) }
//     : 't' { $$ = mparse_spec_generated::makeEscapedLiteralChar($1) }
//
// Quote<std::string>
//     : '\'' { $$ = $1 }
//
// Backslash<std::string>
//     : '\\' { $$ = $1 }

#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace mparse_generated_detail {
    template <typename Value>
    struct Result {
        size_t position = 0;
        Value value{};
    };

    struct PartialResult {
        size_t position = 0;
        std::vector<std::any> stack;
    };

    class PartialGenerator {
    public:
        virtual ~PartialGenerator() = default;
        virtual std::optional<PartialResult> next() = 0;
    };

    class SequentialPartialGenerator : public PartialGenerator {
    public:
        std::optional<PartialResult> next() override {
            while (true) {
                if (current_generator_) {
                    if (auto result = current_generator_->next()) {
                        return result;
                    }
                    current_generator_.reset();
                }
                current_generator_ = makeNextGenerator();
                if (!current_generator_) {
                    return std::nullopt;
                }
            }
        }

    protected:
        virtual std::unique_ptr<PartialGenerator> makeNextGenerator() = 0;

    private:
        std::unique_ptr<PartialGenerator> current_generator_;
    };

    class SinglePartialGenerator final : public PartialGenerator {
    public:
        explicit SinglePartialGenerator(PartialResult result)
            : result_(std::move(result)) {}

        std::optional<PartialResult> next() override {
            if (!result_) {
                return std::nullopt;
            }
            auto result = std::move(result_);
            result_.reset();
            return result;
        }

    private:
        std::optional<PartialResult> result_;
    };

    template <typename Value>
    const Value& semanticValue(const std::vector<std::any>& args, size_t index) {
        return std::any_cast<const Value&>(args.at(index));
    }

    inline void pushUnique(std::vector<size_t>& values, size_t value) {
        for (const auto current : values) {
            if (current == value) {
                return;
            }
        }
        values.push_back(value);
    }
} // namespace mparse_generated_detail

class mparse_parse_AngleContent_generator;
class mparse_parse_AngleContent_vertex_0_generator;
class mparse_parse_AngleContent_vertex_1_generator;
class mparse_parse_AngleContent_vertex_2_generator;
class mparse_parse_AngleContent_vertex_3_generator;
class mparse_parse_AngleContentAfterEmptyPart_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator;
class mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator;
class mparse_parse_AngleContentBeforeEmptyPart_generator;
class mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator;
class mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator;
class mparse_parse_AnglePlainChar_generator;
class mparse_parse_AnglePlainChar_vertex_0_generator;
class mparse_parse_AnglePlainChar_vertex_1_generator;
class mparse_parse_AnglePlainChar_vertex_2_generator;
class mparse_parse_AnglePlainChar_vertex_3_generator;
class mparse_parse_AnglePlainChar_vertex_4_generator;
class mparse_parse_AnglePlainChar_vertex_5_generator;
class mparse_parse_AnglePlainChar_vertex_6_generator;
class mparse_parse_AnglePlainChar_vertex_7_generator;
class mparse_parse_Backslash_generator;
class mparse_parse_Backslash_vertex_0_generator;
class mparse_parse_Backslash_vertex_1_generator;
class mparse_parse_Backslash_vertex_2_generator;
class mparse_parse_BraceContent_generator;
class mparse_parse_BraceContent_vertex_0_generator;
class mparse_parse_BraceContent_vertex_1_generator;
class mparse_parse_BraceContent_vertex_2_generator;
class mparse_parse_BraceContent_vertex_3_generator;
class mparse_parse_BraceContentAfterEmptyPart_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator;
class mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator;
class mparse_parse_BraceContentBeforeEmptyPart_generator;
class mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator;
class mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator;
class mparse_parse_BracePlainChar_generator;
class mparse_parse_BracePlainChar_vertex_0_generator;
class mparse_parse_BracePlainChar_vertex_1_generator;
class mparse_parse_BracePlainChar_vertex_2_generator;
class mparse_parse_BracePlainChar_vertex_3_generator;
class mparse_parse_BracePlainChar_vertex_4_generator;
class mparse_parse_BracePlainChar_vertex_5_generator;
class mparse_parse_BracePlainChar_vertex_6_generator;
class mparse_parse_BracePlainChar_vertex_7_generator;
class mparse_parse_DecimalNumber_generator;
class mparse_parse_DecimalNumber_vertex_0_generator;
class mparse_parse_DecimalNumber_vertex_1_generator;
class mparse_parse_DecimalNumber_vertex_2_generator;
class mparse_parse_DecimalNumber_vertex_3_generator;
class mparse_parse_EscapedLiteralChar_generator;
class mparse_parse_EscapedLiteralChar_vertex_0_generator;
class mparse_parse_EscapedLiteralChar_vertex_1_generator;
class mparse_parse_EscapedLiteralChar_vertex_2_generator;
class mparse_parse_EscapedLiteralChar_vertex_3_generator;
class mparse_parse_EscapedLiteralChar_vertex_4_generator;
class mparse_parse_EscapedLiteralChar_vertex_5_generator;
class mparse_parse_EscapedLiteralChar_vertex_6_generator;
class mparse_parse_HorizontalSpace_generator;
class mparse_parse_HorizontalSpace_vertex_0_generator;
class mparse_parse_HorizontalSpace_vertex_1_generator;
class mparse_parse_HorizontalSpace_vertex_2_generator;
class mparse_parse_HorizontalSpace_vertex_3_generator;
class mparse_parse_HorizontalSpaces_generator;
class mparse_parse_HorizontalSpaces_vertex_0_generator;
class mparse_parse_HorizontalSpaces_vertex_1_generator;
class mparse_parse_HorizontalSpaces_vertex_2_generator;
class mparse_parse_HorizontalSpaces_vertex_3_generator;
class mparse_parse_HorizontalSpaces_vertex_4_generator;
class mparse_parse_HorizontalWhitespace_generator;
class mparse_parse_HorizontalWhitespace_vertex_0_generator;
class mparse_parse_HorizontalWhitespace_vertex_1_generator;
class mparse_parse_HorizontalWhitespace_vertex_2_generator;
class mparse_parse_HorizontalWhitespace_vertex_3_generator;
class mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator;
class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator;
class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator;
class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator;
class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator;
class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator;
class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator;
class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator;
class mparse_parse_Identifier_generator;
class mparse_parse_Identifier_vertex_0_generator;
class mparse_parse_Identifier_vertex_1_generator;
class mparse_parse_Identifier_vertex_2_generator;
class mparse_parse_Identifier_vertex_3_generator;
class mparse_parse_IdentifierPart_generator;
class mparse_parse_IdentifierPart_vertex_0_generator;
class mparse_parse_IdentifierPart_vertex_1_generator;
class mparse_parse_IdentifierPart_vertex_2_generator;
class mparse_parse_IdentifierPart_vertex_3_generator;
class mparse_parse_IdentifierStart_generator;
class mparse_parse_IdentifierStart_vertex_0_generator;
class mparse_parse_IdentifierStart_vertex_1_generator;
class mparse_parse_IdentifierStart_vertex_2_generator;
class mparse_parse_IdentifierStart_vertex_3_generator;
class mparse_parse_IdentifierStart_vertex_4_generator;
class mparse_parse_LineBreak_generator;
class mparse_parse_LineBreak_vertex_0_generator;
class mparse_parse_LineBreak_vertex_1_generator;
class mparse_parse_LineBreak_vertex_2_generator;
class mparse_parse_Literal_generator;
class mparse_parse_Literal_vertex_0_generator;
class mparse_parse_Literal_vertex_1_generator;
class mparse_parse_Literal_vertex_2_generator;
class mparse_parse_Literal_vertex_3_generator;
class mparse_parse_Literal_vertex_4_generator;
class mparse_parse_LiteralChar_generator;
class mparse_parse_LiteralChar_vertex_0_generator;
class mparse_parse_LiteralChar_vertex_1_generator;
class mparse_parse_LiteralChar_vertex_2_generator;
class mparse_parse_LiteralChar_vertex_3_generator;
class mparse_parse_LiteralChar_vertex_4_generator;
class mparse_parse_LiteralChar_vertex_5_generator;
class mparse_parse_LiteralChar_vertex_6_generator;
class mparse_parse_LiteralContent_generator;
class mparse_parse_LiteralContent_vertex_0_generator;
class mparse_parse_LiteralContent_vertex_1_generator;
class mparse_parse_LiteralContent_vertex_2_generator;
class mparse_parse_LiteralContent_vertex_3_generator;
class mparse_parse_LiteralContentAfterEmptyPart_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator;
class mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator;
class mparse_parse_LiteralContentBeforeEmptyPart_generator;
class mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator;
class mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator;
class mparse_parse_LiteralOrRange_generator;
class mparse_parse_LiteralOrRange_vertex_0_generator;
class mparse_parse_LiteralOrRange_vertex_1_generator;
class mparse_parse_LiteralOrRange_vertex_2_generator;
class mparse_parse_LiteralOrRange_vertex_3_generator;
class mparse_parse_LiteralOrRange_vertex_4_generator;
class mparse_parse_LiteralOrRange_vertex_5_generator;
class mparse_parse_LiteralOrRange_vertex_6_generator;
class mparse_parse_LiteralOrRange_vertex_7_generator;
class mparse_parse_LiteralOrRange_vertex_8_generator;
class mparse_parse_LiteralOrRange_vertex_9_generator;
class mparse_parse_LiteralOrRange_vertex_10_generator;
class mparse_parse_LiteralOrRange_vertex_11_generator;
class mparse_parse_ParenContent_generator;
class mparse_parse_ParenContent_vertex_0_generator;
class mparse_parse_ParenContent_vertex_1_generator;
class mparse_parse_ParenContent_vertex_2_generator;
class mparse_parse_ParenContent_vertex_3_generator;
class mparse_parse_ParenContentAfterEmptyPart_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator;
class mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator;
class mparse_parse_ParenContentBeforeEmptyPart_generator;
class mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator;
class mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator;
class mparse_parse_ParenPlainChar_generator;
class mparse_parse_ParenPlainChar_vertex_0_generator;
class mparse_parse_ParenPlainChar_vertex_1_generator;
class mparse_parse_ParenPlainChar_vertex_2_generator;
class mparse_parse_ParenPlainChar_vertex_3_generator;
class mparse_parse_ParenPlainChar_vertex_4_generator;
class mparse_parse_ParenPlainChar_vertex_5_generator;
class mparse_parse_Quote_generator;
class mparse_parse_Quote_vertex_0_generator;
class mparse_parse_Quote_vertex_1_generator;
class mparse_parse_Quote_vertex_2_generator;
class mparse_parse_RepeatCountExpression_generator;
class mparse_parse_RepeatCountExpression_vertex_0_generator;
class mparse_parse_RepeatCountExpression_vertex_1_generator;
class mparse_parse_RepeatCountExpression_vertex_2_generator;
class mparse_parse_RepeatCountExpression_vertex_3_generator;
class mparse_parse_RepeatCountExpression_vertex_4_generator;
class mparse_parse_RepeatCountExpression_vertex_5_generator;
class mparse_parse_RepeatCountExpression_vertex_6_generator;
class mparse_parse_Rule_generator;
class mparse_parse_Rule_vertex_0_generator;
class mparse_parse_Rule_vertex_1_generator;
class mparse_parse_Rule_vertex_2_generator;
class mparse_parse_Rule_vertex_3_generator;
class mparse_parse_Rule_vertex_4_generator;
class mparse_parse_RuleBody_generator;
class mparse_parse_RuleBody_vertex_0_generator;
class mparse_parse_RuleBody_vertex_1_generator;
class mparse_parse_RuleBody_vertex_2_generator;
class mparse_parse_RuleBody_vertex_3_generator;
class mparse_parse_RuleBodyAfterEmptyPart_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator;
class mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator;
class mparse_parse_RuleBodyBeforeEmptyPart_generator;
class mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator;
class mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator;
class mparse_parse_RuleItem_generator;
class mparse_parse_RuleItem_vertex_0_generator;
class mparse_parse_RuleItem_vertex_1_generator;
class mparse_parse_RuleItem_vertex_2_generator;
class mparse_parse_RuleItem_vertex_3_generator;
class mparse_parse_RuleTail_generator;
class mparse_parse_RuleTail_vertex_0_generator;
class mparse_parse_RuleTail_vertex_1_generator;
class mparse_parse_RuleTail_vertex_2_generator;
class mparse_parse_RuleTail_vertex_3_generator;
class mparse_parse_RuleTailAfterEmptyPart_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator;
class mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator;
class mparse_parse_RuleTailAfterSpaces_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_0_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_1_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_2_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_3_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_4_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_5_generator;
class mparse_parse_RuleTailAfterSpaces_vertex_6_generator;
class mparse_parse_RuleTailBeforeEmptyPart_generator;
class mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator;
class mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator;
class mparse_parse_Rules_generator;
class mparse_parse_Rules_vertex_0_generator;
class mparse_parse_Rules_vertex_1_generator;
class mparse_parse_Rules_vertex_2_generator;
class mparse_parse_Rules_vertex_3_generator;
class mparse_parse_Rules_vertex_4_generator;
class mparse_parse_Rules_vertex_5_generator;
class mparse_parse_Rules_vertex_6_generator;
class mparse_parse_Rules_vertex_7_generator;
class mparse_parse_Spec_generator;
class mparse_parse_Spec_vertex_0_generator;
class mparse_parse_Spec_vertex_1_generator;
class mparse_parse_Spec_vertex_2_generator;
class mparse_parse_Spec_vertex_3_generator;
class mparse_parse_Spec_vertex_4_generator;
class mparse_parse_Spec_vertex_5_generator;
class mparse_parse_Spec_vertex_6_generator;
class mparse_parse_Spec_vertex_7_generator;
class mparse_parse_Spec_vertex_8_generator;
class mparse_parse_Spec_vertex_9_generator;
class mparse_parse_SquareContent_generator;
class mparse_parse_SquareContent_vertex_0_generator;
class mparse_parse_SquareContent_vertex_1_generator;
class mparse_parse_SquareContent_vertex_2_generator;
class mparse_parse_SquareContent_vertex_3_generator;
class mparse_parse_SquareContentAfterEmptyPart_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator;
class mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator;
class mparse_parse_SquareContentBeforeEmptyPart_generator;
class mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator;
class mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator;
class mparse_parse_SquarePlainChar_generator;
class mparse_parse_SquarePlainChar_vertex_0_generator;
class mparse_parse_SquarePlainChar_vertex_1_generator;
class mparse_parse_SquarePlainChar_vertex_2_generator;
class mparse_parse_SquarePlainChar_vertex_3_generator;
class mparse_parse_SquarePlainChar_vertex_4_generator;
class mparse_parse_SquarePlainChar_vertex_5_generator;
class mparse_parse_SquarePlainChar_vertex_6_generator;
class mparse_parse_SquarePlainChar_vertex_7_generator;
class mparse_parse_Symbol_generator;
class mparse_parse_Symbol_vertex_0_generator;
class mparse_parse_Symbol_vertex_1_generator;
class mparse_parse_Symbol_vertex_2_generator;
class mparse_parse_Symbol_vertex_3_generator;
class mparse_parse_Symbol_vertex_4_generator;
class mparse_parse_Symbol_vertex_5_generator;
class mparse_parse_Symbol_vertex_6_generator;
class mparse_parse_Symbol_vertex_7_generator;
class mparse_parse_Symbol_vertex_8_generator;
class mparse_parse_Symbol_vertex_9_generator;
class mparse_parse_Symbol_vertex_10_generator;
class mparse_parse_Symbol_vertex_11_generator;
class mparse_parse_Symbol_vertex_12_generator;
class mparse_parse_Symbol_vertex_13_generator;
class mparse_parse_Symbol_vertex_14_generator;
class mparse_parse_Symbol_vertex_15_generator;
class mparse_parse_Symbol_vertex_16_generator;
class mparse_parse_Symbol_vertex_17_generator;
class mparse_parse_Symbol_vertex_18_generator;
class mparse_parse_Symbol_vertex_19_generator;
class mparse_parse_Symbol_vertex_20_generator;
class mparse_parse_Symbol_vertex_21_generator;
class mparse_parse_Symbol_vertex_22_generator;
class mparse_parse_Symbol_vertex_23_generator;
class mparse_parse_Symbol_vertex_24_generator;
class mparse_parse_Symbol_vertex_25_generator;
class mparse_parse_Symbol_vertex_26_generator;
class mparse_parse_Symbol_vertex_27_generator;
class mparse_parse_Symbol_vertex_28_generator;
class mparse_parse_Symbol_vertex_29_generator;
class mparse_parse_Symbol_vertex_30_generator;
class mparse_parse_Symbol_vertex_31_generator;
class mparse_parse_Symbol_vertex_32_generator;
class mparse_parse_Symbol_vertex_33_generator;
class mparse_parse_SymbolReference_generator;
class mparse_parse_SymbolReference_vertex_0_generator;
class mparse_parse_SymbolReference_vertex_1_generator;
class mparse_parse_SymbolReference_vertex_2_generator;
class mparse_parse_SymbolReference_vertex_3_generator;
class mparse_parse_SymbolReference_vertex_4_generator;
class mparse_parse_SymbolReference_vertex_5_generator;
class mparse_parse_SymbolReference_vertex_6_generator;
class mparse_parse_SymbolReference_vertex_7_generator;
class mparse_parse_Symbols_generator;
class mparse_parse_Symbols_vertex_0_generator;
class mparse_parse_Symbols_vertex_1_generator;
class mparse_parse_Symbols_vertex_2_generator;
class mparse_parse_Symbols_vertex_3_generator;
class mparse_parse_Symbols_vertex_4_generator;
class mparse_parse_Symbols_vertex_5_generator;
class mparse_parse_Symbols_vertex_6_generator;
class mparse_parse_Symbols_vertex_7_generator;
class mparse_parse_Whitespace_generator;
class mparse_parse_Whitespace_vertex_0_generator;
class mparse_parse_Whitespace_vertex_1_generator;
class mparse_parse_Whitespace_vertex_2_generator;
class mparse_parse_Whitespace_vertex_3_generator;
class mparse_parse_WhitespaceAfterEmptyPart_generator;
class mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator;
class mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator;
class mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator;
class mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator;
class mparse_parse_WhitespaceBeforeEmptyPart_generator;
class mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator;
class mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator;
class mparse_parse_WhitespaceChar_generator;
class mparse_parse_WhitespaceChar_vertex_0_generator;
class mparse_parse_WhitespaceChar_vertex_1_generator;
class mparse_parse_WhitespaceChar_vertex_2_generator;
class mparse_parse_WhitespaceChar_vertex_3_generator;

static std::any mparse_action_0(const std::vector<std::any>& args);
static std::any mparse_action_1(const std::vector<std::any>& args);
static std::any mparse_action_2(const std::vector<std::any>& args);
static std::any mparse_action_3(const std::vector<std::any>& args);
static std::any mparse_action_4(const std::vector<std::any>& args);
static std::any mparse_action_5(const std::vector<std::any>& args);
static std::any mparse_action_6(const std::vector<std::any>& args);
static std::any mparse_action_7(const std::vector<std::any>& args);
static std::any mparse_action_8(const std::vector<std::any>& args);
static std::any mparse_action_9(const std::vector<std::any>& args);
static std::any mparse_action_10(const std::vector<std::any>& args);
static std::any mparse_action_11(const std::vector<std::any>& args);
static std::any mparse_action_12(const std::vector<std::any>& args);
static std::any mparse_action_13(const std::vector<std::any>& args);
static std::any mparse_action_14(const std::vector<std::any>& args);
static std::any mparse_action_15(const std::vector<std::any>& args);
static std::any mparse_action_16(const std::vector<std::any>& args);
static std::any mparse_action_17(const std::vector<std::any>& args);
static std::any mparse_action_18(const std::vector<std::any>& args);
static std::any mparse_action_19(const std::vector<std::any>& args);
static std::any mparse_action_20(const std::vector<std::any>& args);
static std::any mparse_action_21(const std::vector<std::any>& args);
static std::any mparse_action_22(const std::vector<std::any>& args);
static std::any mparse_action_23(const std::vector<std::any>& args);
static std::any mparse_action_24(const std::vector<std::any>& args);
static std::any mparse_action_25(const std::vector<std::any>& args);
static std::any mparse_action_26(const std::vector<std::any>& args);
static std::any mparse_action_27(const std::vector<std::any>& args);
static std::any mparse_action_28(const std::vector<std::any>& args);
static std::any mparse_action_29(const std::vector<std::any>& args);
static std::any mparse_action_30(const std::vector<std::any>& args);
static std::any mparse_action_31(const std::vector<std::any>& args);
static std::any mparse_action_32(const std::vector<std::any>& args);
static std::any mparse_action_33(const std::vector<std::any>& args);
static std::any mparse_action_34(const std::vector<std::any>& args);
static std::any mparse_action_35(const std::vector<std::any>& args);
static std::any mparse_action_36(const std::vector<std::any>& args);
static std::any mparse_action_37(const std::vector<std::any>& args);
static std::any mparse_action_38(const std::vector<std::any>& args);
static std::any mparse_action_39(const std::vector<std::any>& args);
static std::any mparse_action_40(const std::vector<std::any>& args);
static std::any mparse_action_41(const std::vector<std::any>& args);
static std::any mparse_action_42(const std::vector<std::any>& args);
static std::any mparse_action_43(const std::vector<std::any>& args);
static std::any mparse_action_44(const std::vector<std::any>& args);
static std::any mparse_action_45(const std::vector<std::any>& args);
static std::any mparse_action_46(const std::vector<std::any>& args);
static std::any mparse_action_47(const std::vector<std::any>& args);
static std::any mparse_action_48(const std::vector<std::any>& args);
static std::any mparse_action_49(const std::vector<std::any>& args);
static std::any mparse_action_50(const std::vector<std::any>& args);
static std::any mparse_action_51(const std::vector<std::any>& args);
static std::any mparse_action_52(const std::vector<std::any>& args);
static std::any mparse_action_53(const std::vector<std::any>& args);
static std::any mparse_action_54(const std::vector<std::any>& args);
static std::any mparse_action_55(const std::vector<std::any>& args);
static std::any mparse_action_56(const std::vector<std::any>& args);
static std::any mparse_action_57(const std::vector<std::any>& args);
static std::any mparse_action_58(const std::vector<std::any>& args);
static std::any mparse_action_59(const std::vector<std::any>& args);
static std::any mparse_action_60(const std::vector<std::any>& args);
static std::any mparse_action_61(const std::vector<std::any>& args);
static std::any mparse_action_62(const std::vector<std::any>& args);
static std::any mparse_action_63(const std::vector<std::any>& args);
static std::any mparse_action_64(const std::vector<std::any>& args);
static std::any mparse_action_65(const std::vector<std::any>& args);
static std::any mparse_action_66(const std::vector<std::any>& args);
static std::any mparse_action_67(const std::vector<std::any>& args);
static std::any mparse_action_68(const std::vector<std::any>& args);
static std::any mparse_action_69(const std::vector<std::any>& args);
static std::any mparse_action_70(const std::vector<std::any>& args);
static std::any mparse_action_71(const std::vector<std::any>& args);
static std::any mparse_action_72(const std::vector<std::any>& args);
static std::any mparse_action_73(const std::vector<std::any>& args);
static std::any mparse_action_74(const std::vector<std::any>& args);
static std::any mparse_action_75(const std::vector<std::any>& args);
static std::any mparse_action_76(const std::vector<std::any>& args);
static std::any mparse_action_77(const std::vector<std::any>& args);
static std::any mparse_action_78(const std::vector<std::any>& args);
static std::any mparse_action_79(const std::vector<std::any>& args);
static std::any mparse_action_80(const std::vector<std::any>& args);
static std::any mparse_action_81(const std::vector<std::any>& args);
static std::any mparse_action_82(const std::vector<std::any>& args);
static std::any mparse_action_83(const std::vector<std::any>& args);
static std::any mparse_action_84(const std::vector<std::any>& args);
static std::any mparse_action_85(const std::vector<std::any>& args);
static std::any mparse_action_86(const std::vector<std::any>& args);
static std::any mparse_action_87(const std::vector<std::any>& args);
static std::any mparse_action_88(const std::vector<std::any>& args);
static std::any mparse_action_89(const std::vector<std::any>& args);
static std::any mparse_action_90(const std::vector<std::any>& args);
static std::any mparse_action_91(const std::vector<std::any>& args);
static std::any mparse_action_92(const std::vector<std::any>& args);
static std::any mparse_action_93(const std::vector<std::any>& args);
static std::any mparse_action_94(const std::vector<std::any>& args);
static std::any mparse_action_95(const std::vector<std::any>& args);
static std::any mparse_action_96(const std::vector<std::any>& args);
static std::any mparse_action_97(const std::vector<std::any>& args);
static std::any mparse_action_98(const std::vector<std::any>& args);
static std::any mparse_action_99(const std::vector<std::any>& args);
static std::any mparse_action_100(const std::vector<std::any>& args);
static std::any mparse_action_101(const std::vector<std::any>& args);
static std::any mparse_action_102(const std::vector<std::any>& args);
static std::any mparse_action_103(const std::vector<std::any>& args);
static std::any mparse_action_104(const std::vector<std::any>& args);
static std::any mparse_action_105(const std::vector<std::any>& args);
static std::any mparse_action_106(const std::vector<std::any>& args);
static std::any mparse_action_107(const std::vector<std::any>& args);
static std::any mparse_action_108(const std::vector<std::any>& args);
static std::any mparse_action_109(const std::vector<std::any>& args);
static std::any mparse_action_110(const std::vector<std::any>& args);
static std::any mparse_action_111(const std::vector<std::any>& args);
static std::any mparse_action_112(const std::vector<std::any>& args);
static std::any mparse_action_113(const std::vector<std::any>& args);
static std::any mparse_action_114(const std::vector<std::any>& args);
static std::any mparse_action_115(const std::vector<std::any>& args);
static std::any mparse_action_116(const std::vector<std::any>& args);
static std::any mparse_action_117(const std::vector<std::any>& args);
static std::any mparse_action_118(const std::vector<std::any>& args);
static std::any mparse_action_119(const std::vector<std::any>& args);
static std::any mparse_action_120(const std::vector<std::any>& args);
static std::any mparse_action_121(const std::vector<std::any>& args);
static std::any mparse_action_122(const std::vector<std::any>& args);

static std::any mparse_action_0(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_1(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_2(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_3(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_4(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0).source + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_5(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1) + mparse_generated_detail::semanticValue<std::string>(args, 2) + mparse_generated_detail::semanticValue<std::string>(args, 3);
    return ret;
}

static std::any mparse_action_6(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_7(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_8(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_9(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_10(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_11(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_12(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_13(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_14(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_15(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_16(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_17(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0).source + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_18(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1) + mparse_generated_detail::semanticValue<std::string>(args, 2) + mparse_generated_detail::semanticValue<std::string>(args, 3);
    return ret;
}

static std::any mparse_action_19(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_20(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_21(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_22(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_23(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_24(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_25(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_26(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 1));
    return ret;
}

static std::any mparse_action_27(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeEscapedLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_28(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeEscapedLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_29(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeEscapedLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_30(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeEscapedLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_31(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeEscapedLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_32(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_33(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_34(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_35(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_36(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_37(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_38(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_39(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_40(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_41(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_42(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_43(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_44(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_45(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_46(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_47(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_48(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::makeLiteral(mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 1));
    return ret;
}

static std::any mparse_action_49(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_50(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_51(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_52(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_53(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_54(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::emptyLiteralData();
    return ret;
}

static std::any mparse_action_55(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_56(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_57(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::prependLiteralChar(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 1));
    return ret;
}

static std::any mparse_action_58(const std::vector<std::any>& args) {
    mparse_spec_generated::LiteralData ret{};
    ret = mparse_spec_generated::prependEscapedLiteralChar(mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 2));
    return ret;
}

static std::any mparse_action_59(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_spec_generated::makeRangeItem(mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 3), mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 4));
    return ret;
}

static std::any mparse_action_60(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_spec_generated::makeRepeatedLiteralItem(mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::ExpressionData>(args, 3));
    return ret;
}

static std::any mparse_action_61(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_spec_generated::makeLiteralItem(mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0));
    return ret;
}

static std::any mparse_action_62(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_63(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_64(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_65(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_66(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0).source + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_67(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1) + mparse_generated_detail::semanticValue<std::string>(args, 2) + mparse_generated_detail::semanticValue<std::string>(args, 3);
    return ret;
}

static std::any mparse_action_68(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_69(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_70(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_71(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_72(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_73(const std::vector<std::any>& args) {
    mparse_spec_generated::ExpressionData ret{};
    ret = mparse_spec_generated::makeExpression(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_74(const std::vector<std::any>& args) {
    mparse_spec_generated::ExpressionData ret{};
    ret = mparse_spec_generated::makeExpression(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_75(const std::vector<std::any>& args) {
    mparse_spec_generated::ExpressionData ret{};
    ret = mparse_spec_generated::makeParenthesizedExpression(mparse_generated_detail::semanticValue<std::string>(args, 1));
    return ret;
}

static std::any mparse_action_76(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleData ret{};
    ret = mparse_spec_generated::makeRule(mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleTailData>(args, 2));
    return ret;
}

static std::any mparse_action_77(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::emptyRuleTail();
    return ret;
}

static std::any mparse_action_78(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_79(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_80(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::makeRuleBodyFromAction(mparse_generated_detail::semanticValue<std::string>(args, 1));
    return ret;
}

static std::any mparse_action_81(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::makeRuleBody(mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedRuleItem>(args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleTailData>(args, 1));
    return ret;
}

static std::any mparse_action_82(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedRuleItem>(args, 0);
    return ret;
}

static std::any mparse_action_83(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedRuleItem>(args, 0);
    return ret;
}

static std::any mparse_action_84(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::emptyRuleTail();
    return ret;
}

static std::any mparse_action_85(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_86(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_87(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::makeActionTail(mparse_generated_detail::semanticValue<std::string>(args, 1));
    return ret;
}

static std::any mparse_action_88(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::prependHorizontalSpaces(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleTailData>(args, 1));
    return ret;
}

static std::any mparse_action_89(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::prependRuleItem(mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedRuleItem>(args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleTailData>(args, 1));
    return ret;
}

static std::any mparse_action_90(const std::vector<std::any>& args) {
    mparse_spec_generated::RuleTailData ret{};
    ret = mparse_spec_generated::makeActionTail(mparse_generated_detail::semanticValue<std::string>(args, 1));
    return ret;
}

static std::any mparse_action_91(const std::vector<std::any>& args) {
    mparse_spec_generated::RulesData ret{};
    ret = mparse_spec_generated::makeRules(mparse_generated_detail::semanticValue<mparse_spec_generated::RuleData>(args, 0));
    return ret;
}

static std::any mparse_action_92(const std::vector<std::any>& args) {
    mparse_spec_generated::RulesData ret{};
    ret = mparse_spec_generated::appendRule(mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleData>(args, 2));
    return ret;
}

static std::any mparse_action_93(const std::vector<std::any>& args) {
    std::vector<std::any> resolved_args;
    resolved_args.reserve(3);
    resolved_args.push_back(args.at(0));
    {
        std::vector<std::any> nested_args;
        nested_args.reserve(0);
        resolved_args.push_back(mparse_action_94(nested_args));
    }
    resolved_args.push_back(args.at(1));
    mparse_spec_generated::RulesData ret{};
    ret = mparse_spec_generated::appendRule(mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(resolved_args, 0), mparse_generated_detail::semanticValue<std::string>(resolved_args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::RuleData>(resolved_args, 2));
    return ret;
}

static std::any mparse_action_94(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_95(const std::vector<std::any>& args) {
    std::vector<mparse::spec::Symbol> ret{};
    ret = mparse_spec_generated::finalizeSymbols(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbols>(args, 1));
    return ret;
}

static std::any mparse_action_96(const std::vector<std::any>& args) {
    std::vector<std::any> resolved_args;
    resolved_args.reserve(3);
    {
        std::vector<std::any> nested_args;
        nested_args.reserve(0);
        resolved_args.push_back(mparse_action_94(nested_args));
    }
    resolved_args.push_back(args.at(0));
    resolved_args.push_back(args.at(1));
    std::vector<mparse::spec::Symbol> ret{};
    ret = mparse_spec_generated::finalizeSymbols(mparse_generated_detail::semanticValue<std::string>(resolved_args, 0), mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbols>(resolved_args, 1));
    return ret;
}

static std::any mparse_action_97(const std::vector<std::any>& args) {
    std::string ret{};
    ret = "";
    return ret;
}

static std::any mparse_action_98(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_99(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_100(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_101(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<mparse_spec_generated::LiteralData>(args, 0).source + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_102(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1) + mparse_generated_detail::semanticValue<std::string>(args, 2) + mparse_generated_detail::semanticValue<std::string>(args, 3);
    return ret;
}

static std::any mparse_action_103(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_104(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_105(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_106(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_107(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_108(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_spec_generated::oneChar(mparse_generated_detail::semanticValue<char>(args, 0));
    return ret;
}

static std::any mparse_action_109(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbol ret{};
    ret = mparse_spec_generated::makeSymbol(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 2), mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(args, 3));
    return ret;
}

static std::any mparse_action_110(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbol ret{};
    ret = mparse_spec_generated::makeSymbolWithArguments(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 3), mparse_generated_detail::semanticValue<std::string>(args, 5), mparse_generated_detail::semanticValue<std::string>(args, 6), mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(args, 7));
    return ret;
}

static std::any mparse_action_111(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbol ret{};
    ret = mparse_spec_generated::makeSymbolWithType(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 3), mparse_generated_detail::semanticValue<std::string>(args, 5), mparse_generated_detail::semanticValue<std::string>(args, 6), mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(args, 7));
    return ret;
}

static std::any mparse_action_112(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbol ret{};
    ret = mparse_spec_generated::makeSymbolWithArgumentsAndType(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 3), mparse_generated_detail::semanticValue<std::string>(args, 5), mparse_generated_detail::semanticValue<std::string>(args, 7), mparse_generated_detail::semanticValue<std::string>(args, 9), mparse_generated_detail::semanticValue<std::string>(args, 10), mparse_generated_detail::semanticValue<mparse_spec_generated::RulesData>(args, 11));
    return ret;
}

static std::any mparse_action_113(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_spec_generated::makeSymbolReference(mparse_generated_detail::semanticValue<std::string>(args, 0));
    return ret;
}

static std::any mparse_action_114(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedRuleItem ret{};
    ret = mparse_spec_generated::makeSymbolReference(mparse_generated_detail::semanticValue<std::string>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<std::string>(args, 3));
    return ret;
}

static std::any mparse_action_115(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbols ret{};
    ret = mparse_spec_generated::makeSymbols(mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbol>(args, 0));
    return ret;
}

static std::any mparse_action_116(const std::vector<std::any>& args) {
    mparse_spec_generated::ParsedSymbols ret{};
    ret = mparse_spec_generated::appendSymbol(mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbols>(args, 0), mparse_generated_detail::semanticValue<std::string>(args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbol>(args, 2));
    return ret;
}

static std::any mparse_action_117(const std::vector<std::any>& args) {
    std::vector<std::any> resolved_args;
    resolved_args.reserve(3);
    resolved_args.push_back(args.at(0));
    {
        std::vector<std::any> nested_args;
        nested_args.reserve(0);
        resolved_args.push_back(mparse_action_94(nested_args));
    }
    resolved_args.push_back(args.at(1));
    mparse_spec_generated::ParsedSymbols ret{};
    ret = mparse_spec_generated::appendSymbol(mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbols>(resolved_args, 0), mparse_generated_detail::semanticValue<std::string>(resolved_args, 1), mparse_generated_detail::semanticValue<mparse_spec_generated::ParsedSymbol>(resolved_args, 2));
    return ret;
}

static std::any mparse_action_118(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_119(const std::vector<std::any>& args) {
    return args.at(0);
}

static std::any mparse_action_120(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0) + mparse_generated_detail::semanticValue<std::string>(args, 1);
    return ret;
}

static std::any mparse_action_121(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

static std::any mparse_action_122(const std::vector<std::any>& args) {
    std::string ret{};
    ret = mparse_generated_detail::semanticValue<std::string>(args, 0);
    return ret;
}

class mparse_parse_AngleContent_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_AngleContent_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_AngleContentAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_AngleContentAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_AngleContentBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_AngleContentBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_AnglePlainChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_AnglePlainChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Backslash_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_Backslash_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_BraceContent_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_BraceContent_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_BraceContentAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_BraceContentAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_BraceContentBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_BraceContentBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_BracePlainChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_BracePlainChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_DecimalNumber_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_DecimalNumber_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_EscapedLiteralChar_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::LiteralData>;

    mparse_parse_EscapedLiteralChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_HorizontalSpace_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_HorizontalSpace_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_HorizontalSpaces_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_HorizontalSpaces_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_HorizontalWhitespace_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_HorizontalWhitespace_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Identifier_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_Identifier_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_IdentifierPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_IdentifierPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_IdentifierStart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_IdentifierStart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LineBreak_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_LineBreak_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Literal_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::LiteralData>;

    mparse_parse_Literal_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LiteralChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_LiteralChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LiteralContent_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::LiteralData>;

    mparse_parse_LiteralContent_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LiteralContentAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::LiteralData>;

    mparse_parse_LiteralContentAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LiteralContentBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::LiteralData>;

    mparse_parse_LiteralContentBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_LiteralOrRange_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ParsedRuleItem>;

    mparse_parse_LiteralOrRange_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_ParenContent_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_ParenContent_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_ParenContentAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_ParenContentAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_ParenContentBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_ParenContentBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_ParenPlainChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_ParenPlainChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Quote_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_Quote_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RepeatCountExpression_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ExpressionData>;

    mparse_parse_RepeatCountExpression_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Rule_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleData>;

    mparse_parse_Rule_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleBody_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleBody_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleBodyAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleBodyAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleBodyBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleBodyBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleItem_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ParsedRuleItem>;

    mparse_parse_RuleItem_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleTail_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleTail_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleTailAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleTailAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleTailAfterSpaces_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleTailAfterSpaces_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_RuleTailBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RuleTailData>;

    mparse_parse_RuleTailBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Rules_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::RulesData>;

    mparse_parse_Rules_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Spec_generator {
public:
    using Result = mparse_generated_detail::Result<std::vector<mparse::spec::Symbol>>;

    mparse_parse_Spec_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_SquareContent_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_SquareContent_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_SquareContentAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_SquareContentAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_SquareContentBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_SquareContentBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_SquarePlainChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_SquarePlainChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Symbol_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ParsedSymbol>;

    mparse_parse_Symbol_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_SymbolReference_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ParsedRuleItem>;

    mparse_parse_SymbolReference_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Symbols_generator {
public:
    using Result = mparse_generated_detail::Result<mparse_spec_generated::ParsedSymbols>;

    mparse_parse_Symbols_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_Whitespace_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_Whitespace_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_WhitespaceAfterEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_WhitespaceAfterEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_WhitespaceBeforeEmptyPart_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_WhitespaceBeforeEmptyPart_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_WhitespaceChar_generator {
public:
    using Result = mparse_generated_detail::Result<std::string>;

    mparse_parse_WhitespaceChar_generator(std::string_view input, size_t position);
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

class mparse_parse_AngleContent_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContent_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContent_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContent_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_AnglePlainChar_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_AnglePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Backslash_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Backslash_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Backslash_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Backslash_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Backslash_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Backslash_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContent_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContent_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContent_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContent_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_BracePlainChar_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_BracePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_DecimalNumber_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_DecimalNumber_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_DecimalNumber_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_DecimalNumber_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_DecimalNumber_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_DecimalNumber_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_DecimalNumber_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_DecimalNumber_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_EscapedLiteralChar_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_EscapedLiteralChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpace_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpace_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpace_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpace_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpaces_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpaces_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpaces_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpaces_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpaces_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpaces_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpaces_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpaces_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalSpaces_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalSpaces_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespace_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespace_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespace_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespace_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Identifier_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Identifier_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Identifier_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Identifier_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Identifier_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Identifier_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Identifier_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Identifier_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierStart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierStart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierStart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierStart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierStart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierStart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierStart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierStart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_IdentifierStart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_IdentifierStart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LineBreak_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LineBreak_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LineBreak_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LineBreak_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LineBreak_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LineBreak_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Literal_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Literal_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Literal_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Literal_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Literal_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Literal_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Literal_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Literal_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Literal_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Literal_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralChar_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContent_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContent_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContent_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContent_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_10_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_10_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_LiteralOrRange_vertex_11_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_LiteralOrRange_vertex_11_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContent_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContent_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContent_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContent_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_ParenPlainChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_ParenPlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Quote_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Quote_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Quote_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Quote_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Quote_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Quote_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RepeatCountExpression_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RepeatCountExpression_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rule_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rule_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rule_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rule_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rule_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rule_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rule_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rule_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rule_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rule_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBody_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBody_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBody_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBody_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBody_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBody_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBody_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBody_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleItem_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleItem_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleItem_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleItem_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleItem_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleItem_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleItem_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleItem_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTail_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTail_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTail_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTail_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTail_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTail_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTail_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTail_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailAfterSpaces_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailAfterSpaces_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Rules_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Rules_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Spec_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Spec_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContent_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContent_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContent_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContent_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SquarePlainChar_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SquarePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_8_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_9_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_10_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_10_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_11_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_11_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_12_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_12_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_13_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_13_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_14_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_14_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_15_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_15_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_16_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_16_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_17_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_17_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_18_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_18_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_19_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_19_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_20_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_20_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_21_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_21_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_22_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_22_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_23_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_23_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_24_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_24_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_25_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_25_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_26_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_26_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_27_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_27_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_28_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_28_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_29_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_29_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_30_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_30_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_31_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_31_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_32_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_32_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbol_vertex_33_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbol_vertex_33_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_SymbolReference_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_SymbolReference_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_4_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_5_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_6_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Symbols_vertex_7_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Symbols_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Whitespace_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Whitespace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Whitespace_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Whitespace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Whitespace_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Whitespace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_Whitespace_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_Whitespace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceChar_vertex_0_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceChar_vertex_1_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceChar_vertex_2_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

class mparse_parse_WhitespaceChar_vertex_3_generator : public mparse_generated_detail::SequentialPartialGenerator {
public:
    mparse_parse_WhitespaceChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack);

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

mparse_parse_AngleContent_generator::mparse_parse_AngleContent_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_AngleContent_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_AngleContent_generator::Result> mparse_parse_AngleContent_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_AngleContentAfterEmptyPart_generator::mparse_parse_AngleContentAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_AngleContentAfterEmptyPart_generator::Result> mparse_parse_AngleContentAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_AngleContentBeforeEmptyPart_generator::mparse_parse_AngleContentBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_AngleContentBeforeEmptyPart_generator::Result> mparse_parse_AngleContentBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_AnglePlainChar_generator::mparse_parse_AnglePlainChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_AnglePlainChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_AnglePlainChar_generator::Result> mparse_parse_AnglePlainChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Backslash_generator::mparse_parse_Backslash_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Backslash_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Backslash_generator::Result> mparse_parse_Backslash_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_BraceContent_generator::mparse_parse_BraceContent_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_BraceContent_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_BraceContent_generator::Result> mparse_parse_BraceContent_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_BraceContentAfterEmptyPart_generator::mparse_parse_BraceContentAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_BraceContentAfterEmptyPart_generator::Result> mparse_parse_BraceContentAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_BraceContentBeforeEmptyPart_generator::mparse_parse_BraceContentBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_BraceContentBeforeEmptyPart_generator::Result> mparse_parse_BraceContentBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_BracePlainChar_generator::mparse_parse_BracePlainChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_BracePlainChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_BracePlainChar_generator::Result> mparse_parse_BracePlainChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_DecimalNumber_generator::mparse_parse_DecimalNumber_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_DecimalNumber_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_DecimalNumber_generator::Result> mparse_parse_DecimalNumber_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_EscapedLiteralChar_generator::mparse_parse_EscapedLiteralChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_EscapedLiteralChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_EscapedLiteralChar_generator::Result> mparse_parse_EscapedLiteralChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::LiteralData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_HorizontalSpace_generator::mparse_parse_HorizontalSpace_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_HorizontalSpace_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_HorizontalSpace_generator::Result> mparse_parse_HorizontalSpace_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_HorizontalSpaces_generator::mparse_parse_HorizontalSpaces_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_HorizontalSpaces_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_HorizontalSpaces_generator::Result> mparse_parse_HorizontalSpaces_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_HorizontalWhitespace_generator::mparse_parse_HorizontalWhitespace_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_HorizontalWhitespace_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_HorizontalWhitespace_generator::Result> mparse_parse_HorizontalWhitespace_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator::mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator::Result> mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator::mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator::Result> mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Identifier_generator::mparse_parse_Identifier_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Identifier_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Identifier_generator::Result> mparse_parse_Identifier_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_IdentifierPart_generator::mparse_parse_IdentifierPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_IdentifierPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_IdentifierPart_generator::Result> mparse_parse_IdentifierPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_IdentifierStart_generator::mparse_parse_IdentifierStart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_IdentifierStart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_IdentifierStart_generator::Result> mparse_parse_IdentifierStart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LineBreak_generator::mparse_parse_LineBreak_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LineBreak_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LineBreak_generator::Result> mparse_parse_LineBreak_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Literal_generator::mparse_parse_Literal_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Literal_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Literal_generator::Result> mparse_parse_Literal_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::LiteralData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LiteralChar_generator::mparse_parse_LiteralChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LiteralChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LiteralChar_generator::Result> mparse_parse_LiteralChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LiteralContent_generator::mparse_parse_LiteralContent_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LiteralContent_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LiteralContent_generator::Result> mparse_parse_LiteralContent_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::LiteralData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LiteralContentAfterEmptyPart_generator::mparse_parse_LiteralContentAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LiteralContentAfterEmptyPart_generator::Result> mparse_parse_LiteralContentAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::LiteralData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LiteralContentBeforeEmptyPart_generator::mparse_parse_LiteralContentBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LiteralContentBeforeEmptyPart_generator::Result> mparse_parse_LiteralContentBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::LiteralData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_LiteralOrRange_generator::mparse_parse_LiteralOrRange_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_LiteralOrRange_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_LiteralOrRange_generator::Result> mparse_parse_LiteralOrRange_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ParsedRuleItem>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_ParenContent_generator::mparse_parse_ParenContent_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_ParenContent_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_ParenContent_generator::Result> mparse_parse_ParenContent_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_ParenContentAfterEmptyPart_generator::mparse_parse_ParenContentAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_ParenContentAfterEmptyPart_generator::Result> mparse_parse_ParenContentAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_ParenContentBeforeEmptyPart_generator::mparse_parse_ParenContentBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_ParenContentBeforeEmptyPart_generator::Result> mparse_parse_ParenContentBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_ParenPlainChar_generator::mparse_parse_ParenPlainChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_ParenPlainChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_ParenPlainChar_generator::Result> mparse_parse_ParenPlainChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Quote_generator::mparse_parse_Quote_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Quote_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Quote_generator::Result> mparse_parse_Quote_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RepeatCountExpression_generator::mparse_parse_RepeatCountExpression_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RepeatCountExpression_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RepeatCountExpression_generator::Result> mparse_parse_RepeatCountExpression_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ExpressionData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Rule_generator::mparse_parse_Rule_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Rule_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Rule_generator::Result> mparse_parse_Rule_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleBody_generator::mparse_parse_RuleBody_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleBody_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleBody_generator::Result> mparse_parse_RuleBody_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleBodyAfterEmptyPart_generator::mparse_parse_RuleBodyAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleBodyAfterEmptyPart_generator::Result> mparse_parse_RuleBodyAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleBodyBeforeEmptyPart_generator::mparse_parse_RuleBodyBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleBodyBeforeEmptyPart_generator::Result> mparse_parse_RuleBodyBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleItem_generator::mparse_parse_RuleItem_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleItem_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleItem_generator::Result> mparse_parse_RuleItem_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ParsedRuleItem>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleTail_generator::mparse_parse_RuleTail_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleTail_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleTail_generator::Result> mparse_parse_RuleTail_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleTailAfterEmptyPart_generator::mparse_parse_RuleTailAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleTailAfterEmptyPart_generator::Result> mparse_parse_RuleTailAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleTailAfterSpaces_generator::mparse_parse_RuleTailAfterSpaces_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleTailAfterSpaces_generator::Result> mparse_parse_RuleTailAfterSpaces_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_RuleTailBeforeEmptyPart_generator::mparse_parse_RuleTailBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_RuleTailBeforeEmptyPart_generator::Result> mparse_parse_RuleTailBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RuleTailData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Rules_generator::mparse_parse_Rules_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Rules_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Rules_generator::Result> mparse_parse_Rules_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::RulesData>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Spec_generator::mparse_parse_Spec_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Spec_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Spec_generator::Result> mparse_parse_Spec_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::vector<mparse::spec::Symbol>>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_SquareContent_generator::mparse_parse_SquareContent_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_SquareContent_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_SquareContent_generator::Result> mparse_parse_SquareContent_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_SquareContentAfterEmptyPart_generator::mparse_parse_SquareContentAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_SquareContentAfterEmptyPart_generator::Result> mparse_parse_SquareContentAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_SquareContentBeforeEmptyPart_generator::mparse_parse_SquareContentBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_SquareContentBeforeEmptyPart_generator::Result> mparse_parse_SquareContentBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_SquarePlainChar_generator::mparse_parse_SquarePlainChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_SquarePlainChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_SquarePlainChar_generator::Result> mparse_parse_SquarePlainChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Symbol_generator::mparse_parse_Symbol_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Symbol_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Symbol_generator::Result> mparse_parse_Symbol_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ParsedSymbol>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_SymbolReference_generator::mparse_parse_SymbolReference_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_SymbolReference_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_SymbolReference_generator::Result> mparse_parse_SymbolReference_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ParsedRuleItem>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Symbols_generator::mparse_parse_Symbols_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Symbols_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Symbols_generator::Result> mparse_parse_Symbols_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<mparse_spec_generated::ParsedSymbols>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_Whitespace_generator::mparse_parse_Whitespace_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_Whitespace_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_Whitespace_generator::Result> mparse_parse_Whitespace_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_WhitespaceAfterEmptyPart_generator::mparse_parse_WhitespaceAfterEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_WhitespaceAfterEmptyPart_generator::Result> mparse_parse_WhitespaceAfterEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_WhitespaceBeforeEmptyPart_generator::mparse_parse_WhitespaceBeforeEmptyPart_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_WhitespaceBeforeEmptyPart_generator::Result> mparse_parse_WhitespaceBeforeEmptyPart_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_WhitespaceChar_generator::mparse_parse_WhitespaceChar_generator(std::string_view input, size_t position)
    : partial_generator_(std::make_unique<mparse_parse_WhitespaceChar_vertex_0_generator>(input, position, std::vector<std::any>{})) {}

std::optional<mparse_parse_WhitespaceChar_generator::Result> mparse_parse_WhitespaceChar_generator::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<std::string>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

mparse_parse_AngleContent_vertex_0_generator::mparse_parse_AngleContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContent_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContentBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_0(stack_)};
            return std::make_unique<mparse_parse_AngleContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContent_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContentAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContent_vertex_1_generator::mparse_parse_AngleContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_AngleContent_vertex_2_generator::mparse_parse_AngleContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_1(stack_)};
            return std::make_unique<mparse_parse_AngleContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContent_vertex_3_generator::mparse_parse_AngleContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContent_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_2(stack_)};
            return std::make_unique<mparse_parse_AngleContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AnglePlainChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            const std::string_view literal = "<";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_3(stack_)};
            return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_4(stack_)};
            return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ">";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator::mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentAfterEmptyPart_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_5(stack_)};
            return std::make_unique<mparse_parse_AngleContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator::mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator::mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AngleContentBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_0_generator::mparse_parse_AnglePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 6) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AnglePlainChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_AnglePlainChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            if (!(position_ < input_.size() && input_[position_] >= '!' && input_[position_] <= '&')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_4_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 3: {
            if (!(position_ < input_.size() && input_[position_] >= '(' && input_[position_] <= ';')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_5_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 4: {
            const std::string_view literal = "=";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 5: {
            if (!(position_ < input_.size() && input_[position_] >= '?' && input_[position_] <= '~')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_7_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_1_generator::mparse_parse_AnglePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_2_generator::mparse_parse_AnglePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_6(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_3_generator::mparse_parse_AnglePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_7(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_4_generator::mparse_parse_AnglePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_8(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_5_generator::mparse_parse_AnglePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_9(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_6_generator::mparse_parse_AnglePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_10(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_AnglePlainChar_vertex_7_generator::mparse_parse_AnglePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_AnglePlainChar_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_11(stack_)};
            return std::make_unique<mparse_parse_AnglePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Backslash_vertex_0_generator::mparse_parse_Backslash_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "\\";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Backslash_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Backslash_vertex_1_generator::mparse_parse_Backslash_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Backslash_vertex_2_generator::mparse_parse_Backslash_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Backslash_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_12(stack_)};
            return std::make_unique<mparse_parse_Backslash_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContent_vertex_0_generator::mparse_parse_BraceContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContent_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContentBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_13(stack_)};
            return std::make_unique<mparse_parse_BraceContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContent_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContentAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContent_vertex_1_generator::mparse_parse_BraceContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_BraceContent_vertex_2_generator::mparse_parse_BraceContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_14(stack_)};
            return std::make_unique<mparse_parse_BraceContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContent_vertex_3_generator::mparse_parse_BraceContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContent_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_15(stack_)};
            return std::make_unique<mparse_parse_BraceContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BracePlainChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            const std::string_view literal = "{";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_16(stack_)};
            return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_17(stack_)};
            return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "}";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator::mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentAfterEmptyPart_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_18(stack_)};
            return std::make_unique<mparse_parse_BraceContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator::mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator::mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BraceContentBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_0_generator::mparse_parse_BracePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 6) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BracePlainChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_BracePlainChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            if (!(position_ < input_.size() && input_[position_] >= '!' && input_[position_] <= '&')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_BracePlainChar_vertex_4_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 3: {
            if (!(position_ < input_.size() && input_[position_] >= '(' && input_[position_] <= 'z')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_BracePlainChar_vertex_5_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 4: {
            const std::string_view literal = "|";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_BracePlainChar_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 5: {
            const std::string_view literal = "~";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_BracePlainChar_vertex_7_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_1_generator::mparse_parse_BracePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_2_generator::mparse_parse_BracePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_19(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_3_generator::mparse_parse_BracePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_20(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_4_generator::mparse_parse_BracePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_21(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_5_generator::mparse_parse_BracePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_22(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_6_generator::mparse_parse_BracePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_23(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_BracePlainChar_vertex_7_generator::mparse_parse_BracePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_BracePlainChar_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_24(stack_)};
            return std::make_unique<mparse_parse_BracePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_DecimalNumber_vertex_0_generator::mparse_parse_DecimalNumber_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            if (!(position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_DecimalNumber_vertex_2_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_DecimalNumber_vertex_1_generator::mparse_parse_DecimalNumber_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            if (!(position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_DecimalNumber_vertex_3_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_DecimalNumber_vertex_2_generator::mparse_parse_DecimalNumber_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_25(stack_)};
            return std::make_unique<mparse_parse_DecimalNumber_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_DecimalNumber_vertex_3_generator::mparse_parse_DecimalNumber_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_DecimalNumber_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_26(stack_)};
            return std::make_unique<mparse_parse_DecimalNumber_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_0_generator::mparse_parse_EscapedLiteralChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 5) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Quote_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Backslash_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 3: {
            const std::string_view literal = "n";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_5_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 4: {
            const std::string_view literal = "t";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_1_generator::mparse_parse_EscapedLiteralChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_2_generator::mparse_parse_EscapedLiteralChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_27(stack_)};
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_3_generator::mparse_parse_EscapedLiteralChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_28(stack_)};
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_4_generator::mparse_parse_EscapedLiteralChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_29(stack_)};
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_5_generator::mparse_parse_EscapedLiteralChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_30(stack_)};
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_EscapedLiteralChar_vertex_6_generator::mparse_parse_EscapedLiteralChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_EscapedLiteralChar_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_31(stack_)};
            return std::make_unique<mparse_parse_EscapedLiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpace_vertex_0_generator::mparse_parse_HorizontalSpace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = " ";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_HorizontalSpace_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 1: {
            const std::string_view literal = "\t";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_HorizontalSpace_vertex_3_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpace_vertex_1_generator::mparse_parse_HorizontalSpace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpace_vertex_2_generator::mparse_parse_HorizontalSpace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_32(stack_)};
            return std::make_unique<mparse_parse_HorizontalSpace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpace_vertex_3_generator::mparse_parse_HorizontalSpace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpace_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_33(stack_)};
            return std::make_unique<mparse_parse_HorizontalSpace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpaces_vertex_0_generator::mparse_parse_HorizontalSpaces_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalSpaces_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalSpaces_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpaces_vertex_1_generator::mparse_parse_HorizontalSpaces_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpaces_vertex_2_generator::mparse_parse_HorizontalSpaces_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_34(stack_)};
            return std::make_unique<mparse_parse_HorizontalSpaces_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpaces_vertex_3_generator::mparse_parse_HorizontalSpaces_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalSpaces_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpaces_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalSpaces_vertex_4_generator::mparse_parse_HorizontalSpaces_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalSpaces_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_35(stack_)};
            return std::make_unique<mparse_parse_HorizontalSpaces_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespace_vertex_0_generator::mparse_parse_HorizontalWhitespace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalWhitespace_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespaceBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_36(stack_)};
            return std::make_unique<mparse_parse_HorizontalWhitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalWhitespace_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespaceAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespace_vertex_1_generator::mparse_parse_HorizontalWhitespace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespace_vertex_2_generator::mparse_parse_HorizontalWhitespace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_37(stack_)};
            return std::make_unique<mparse_parse_HorizontalWhitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespace_vertex_3_generator::mparse_parse_HorizontalWhitespace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespace_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_38(stack_)};
            return std::make_unique<mparse_parse_HorizontalWhitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator::mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator::mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator::mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator::mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_39(stack_)};
            return std::make_unique<mparse_parse_HorizontalWhitespaceAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator::mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator::mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_HorizontalWhitespaceBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Identifier_vertex_0_generator::mparse_parse_Identifier_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Identifier_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_IdentifierStart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Identifier_vertex_1_generator::mparse_parse_Identifier_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Identifier_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_IdentifierPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Identifier_vertex_2_generator::mparse_parse_Identifier_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_40(stack_)};
            return std::make_unique<mparse_parse_Identifier_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Identifier_vertex_3_generator::mparse_parse_Identifier_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Identifier_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_41(stack_)};
            return std::make_unique<mparse_parse_Identifier_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierPart_vertex_0_generator::mparse_parse_IdentifierPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_IdentifierPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_IdentifierStart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            if (!(position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_IdentifierPart_vertex_3_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierPart_vertex_1_generator::mparse_parse_IdentifierPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierPart_vertex_2_generator::mparse_parse_IdentifierPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_42(stack_)};
            return std::make_unique<mparse_parse_IdentifierPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierPart_vertex_3_generator::mparse_parse_IdentifierPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_43(stack_)};
            return std::make_unique<mparse_parse_IdentifierPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierStart_vertex_0_generator::mparse_parse_IdentifierStart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            if (!(position_ < input_.size() && input_[position_] >= 'a' && input_[position_] <= 'z')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_IdentifierStart_vertex_2_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 1: {
            if (!(position_ < input_.size() && input_[position_] >= 'A' && input_[position_] <= 'Z')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_IdentifierStart_vertex_3_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 2: {
            const std::string_view literal = "_";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_IdentifierStart_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierStart_vertex_1_generator::mparse_parse_IdentifierStart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierStart_vertex_2_generator::mparse_parse_IdentifierStart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_44(stack_)};
            return std::make_unique<mparse_parse_IdentifierStart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierStart_vertex_3_generator::mparse_parse_IdentifierStart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_45(stack_)};
            return std::make_unique<mparse_parse_IdentifierStart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_IdentifierStart_vertex_4_generator::mparse_parse_IdentifierStart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_IdentifierStart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_46(stack_)};
            return std::make_unique<mparse_parse_IdentifierStart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LineBreak_vertex_0_generator::mparse_parse_LineBreak_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "\n";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_LineBreak_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LineBreak_vertex_1_generator::mparse_parse_LineBreak_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LineBreak_vertex_2_generator::mparse_parse_LineBreak_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LineBreak_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_47(stack_)};
            return std::make_unique<mparse_parse_LineBreak_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Literal_vertex_0_generator::mparse_parse_Literal_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Literal_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Quote_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Literal_vertex_1_generator::mparse_parse_Literal_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Literal_vertex_2_generator::mparse_parse_Literal_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Literal_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Literal_vertex_3_generator::mparse_parse_Literal_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Literal_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Quote_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Literal_vertex_4_generator::mparse_parse_Literal_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Literal_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_48(stack_)};
            return std::make_unique<mparse_parse_Literal_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_0_generator::mparse_parse_LiteralChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 5) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            if (!(position_ < input_.size() && input_[position_] >= '!' && input_[position_] <= '&')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_LiteralChar_vertex_4_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 3: {
            if (!(position_ < input_.size() && input_[position_] >= '(' && input_[position_] <= '[')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_LiteralChar_vertex_5_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 4: {
            if (!(position_ < input_.size() && input_[position_] >= ']' && input_[position_] <= '~')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_LiteralChar_vertex_6_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_1_generator::mparse_parse_LiteralChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_2_generator::mparse_parse_LiteralChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_49(stack_)};
            return std::make_unique<mparse_parse_LiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_3_generator::mparse_parse_LiteralChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_50(stack_)};
            return std::make_unique<mparse_parse_LiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_4_generator::mparse_parse_LiteralChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_51(stack_)};
            return std::make_unique<mparse_parse_LiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_5_generator::mparse_parse_LiteralChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_52(stack_)};
            return std::make_unique<mparse_parse_LiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralChar_vertex_6_generator::mparse_parse_LiteralChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralChar_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_53(stack_)};
            return std::make_unique<mparse_parse_LiteralChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContent_vertex_0_generator::mparse_parse_LiteralContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContent_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralContentBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_54(stack_)};
            return std::make_unique<mparse_parse_LiteralContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContent_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralContentAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContent_vertex_1_generator::mparse_parse_LiteralContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContent_vertex_2_generator::mparse_parse_LiteralContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_55(stack_)};
            return std::make_unique<mparse_parse_LiteralContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContent_vertex_3_generator::mparse_parse_LiteralContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContent_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_56(stack_)};
            return std::make_unique<mparse_parse_LiteralContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Backslash_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_57(stack_)};
            return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_EscapedLiteralChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator::mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_58(stack_)};
            return std::make_unique<mparse_parse_LiteralContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator::mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator::mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralContentBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_0_generator::mparse_parse_LiteralOrRange_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_11_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_1_generator::mparse_parse_LiteralOrRange_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_2_generator::mparse_parse_LiteralOrRange_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_3_generator::mparse_parse_LiteralOrRange_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "-";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_LiteralOrRange_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_4_generator::mparse_parse_LiteralOrRange_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_5_generator::mparse_parse_LiteralOrRange_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_6_generator::mparse_parse_LiteralOrRange_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_59(stack_)};
            return std::make_unique<mparse_parse_LiteralOrRange_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_7_generator::mparse_parse_LiteralOrRange_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_8_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_8_generator::mparse_parse_LiteralOrRange_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "^";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_LiteralOrRange_vertex_9_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_9_generator::mparse_parse_LiteralOrRange_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_LiteralOrRange_vertex_10_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RepeatCountExpression_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_10_generator::mparse_parse_LiteralOrRange_vertex_10_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_10_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_10_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_60(stack_)};
            return std::make_unique<mparse_parse_LiteralOrRange_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_LiteralOrRange_vertex_11_generator::mparse_parse_LiteralOrRange_vertex_11_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_11_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_LiteralOrRange_vertex_11_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_61(stack_)};
            return std::make_unique<mparse_parse_LiteralOrRange_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContent_vertex_0_generator::mparse_parse_ParenContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContent_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContentBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_62(stack_)};
            return std::make_unique<mparse_parse_ParenContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContent_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContentAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContent_vertex_1_generator::mparse_parse_ParenContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_ParenContent_vertex_2_generator::mparse_parse_ParenContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_63(stack_)};
            return std::make_unique<mparse_parse_ParenContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContent_vertex_3_generator::mparse_parse_ParenContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContent_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_64(stack_)};
            return std::make_unique<mparse_parse_ParenContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenPlainChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            const std::string_view literal = "(";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_65(stack_)};
            return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_66(stack_)};
            return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ")";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator::mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentAfterEmptyPart_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_67(stack_)};
            return std::make_unique<mparse_parse_ParenContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator::mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator::mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenContentBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_0_generator::mparse_parse_ParenPlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 4) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenPlainChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_ParenPlainChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            if (!(position_ < input_.size() && input_[position_] >= '!' && input_[position_] <= '&')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_4_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 3: {
            if (!(position_ < input_.size() && input_[position_] >= '*' && input_[position_] <= '~')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_5_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_1_generator::mparse_parse_ParenPlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_2_generator::mparse_parse_ParenPlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_68(stack_)};
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_3_generator::mparse_parse_ParenPlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_69(stack_)};
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_4_generator::mparse_parse_ParenPlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_70(stack_)};
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_ParenPlainChar_vertex_5_generator::mparse_parse_ParenPlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_ParenPlainChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_71(stack_)};
            return std::make_unique<mparse_parse_ParenPlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Quote_vertex_0_generator::mparse_parse_Quote_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "'";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Quote_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Quote_vertex_1_generator::mparse_parse_Quote_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Quote_vertex_2_generator::mparse_parse_Quote_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Quote_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_72(stack_)};
            return std::make_unique<mparse_parse_Quote_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_0_generator::mparse_parse_RepeatCountExpression_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RepeatCountExpression_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RepeatCountExpression_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_DecimalNumber_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            const std::string_view literal = "(";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RepeatCountExpression_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_1_generator::mparse_parse_RepeatCountExpression_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_2_generator::mparse_parse_RepeatCountExpression_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_73(stack_)};
            return std::make_unique<mparse_parse_RepeatCountExpression_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_3_generator::mparse_parse_RepeatCountExpression_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_74(stack_)};
            return std::make_unique<mparse_parse_RepeatCountExpression_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_4_generator::mparse_parse_RepeatCountExpression_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RepeatCountExpression_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_ParenContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_5_generator::mparse_parse_RepeatCountExpression_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ")";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RepeatCountExpression_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RepeatCountExpression_vertex_6_generator::mparse_parse_RepeatCountExpression_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RepeatCountExpression_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_75(stack_)};
            return std::make_unique<mparse_parse_RepeatCountExpression_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rule_vertex_0_generator::mparse_parse_Rule_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ":";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Rule_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rule_vertex_1_generator::mparse_parse_Rule_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Rule_vertex_2_generator::mparse_parse_Rule_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rule_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rule_vertex_3_generator::mparse_parse_Rule_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rule_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleBody_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rule_vertex_4_generator::mparse_parse_Rule_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rule_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_76(stack_)};
            return std::make_unique<mparse_parse_Rule_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBody_vertex_0_generator::mparse_parse_RuleBody_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleBody_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleBodyBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_77(stack_)};
            return std::make_unique<mparse_parse_RuleBody_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleBody_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleBodyAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBody_vertex_1_generator::mparse_parse_RuleBody_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleBody_vertex_2_generator::mparse_parse_RuleBody_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_78(stack_)};
            return std::make_unique<mparse_parse_RuleBody_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBody_vertex_3_generator::mparse_parse_RuleBody_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBody_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_79(stack_)};
            return std::make_unique<mparse_parse_RuleBody_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "{";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleItem_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "}";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_80(stack_)};
            return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleTail_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator::mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_81(stack_)};
            return std::make_unique<mparse_parse_RuleBodyAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator::mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator::mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleBodyBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleItem_vertex_0_generator::mparse_parse_RuleItem_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleItem_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LiteralOrRange_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleItem_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SymbolReference_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleItem_vertex_1_generator::mparse_parse_RuleItem_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleItem_vertex_2_generator::mparse_parse_RuleItem_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_82(stack_)};
            return std::make_unique<mparse_parse_RuleItem_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleItem_vertex_3_generator::mparse_parse_RuleItem_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleItem_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_83(stack_)};
            return std::make_unique<mparse_parse_RuleItem_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTail_vertex_0_generator::mparse_parse_RuleTail_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTail_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleTailBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_84(stack_)};
            return std::make_unique<mparse_parse_RuleTail_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTail_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleTailAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTail_vertex_1_generator::mparse_parse_RuleTail_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleTail_vertex_2_generator::mparse_parse_RuleTail_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_85(stack_)};
            return std::make_unique<mparse_parse_RuleTail_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTail_vertex_3_generator::mparse_parse_RuleTail_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTail_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_86(stack_)};
            return std::make_unique<mparse_parse_RuleTail_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "{";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpaces_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "}";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_87(stack_)};
            return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleTailAfterSpaces_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator::mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_88(stack_)};
            return std::make_unique<mparse_parse_RuleTailAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_0_generator::mparse_parse_RuleTailAfterSpaces_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleItem_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            const std::string_view literal = "{";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_4_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_1_generator::mparse_parse_RuleTailAfterSpaces_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_2_generator::mparse_parse_RuleTailAfterSpaces_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_RuleTail_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_3_generator::mparse_parse_RuleTailAfterSpaces_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_89(stack_)};
            return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_4_generator::mparse_parse_RuleTailAfterSpaces_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_BraceContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_5_generator::mparse_parse_RuleTailAfterSpaces_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "}";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailAfterSpaces_vertex_6_generator::mparse_parse_RuleTailAfterSpaces_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailAfterSpaces_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_90(stack_)};
            return std::make_unique<mparse_parse_RuleTailAfterSpaces_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator::mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator::mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_RuleTailBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_0_generator::mparse_parse_Rules_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rule_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_1_generator::mparse_parse_Rules_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rule_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_2_generator::mparse_parse_Rules_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_91(stack_)};
            return std::make_unique<mparse_parse_Rules_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_3_generator::mparse_parse_Rules_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rule_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_4_generator::mparse_parse_Rules_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_92(stack_)};
            return std::make_unique<mparse_parse_Rules_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_5_generator::mparse_parse_Rules_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_93(stack_)};
            return std::make_unique<mparse_parse_Rules_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_6_generator::mparse_parse_Rules_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Rules_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rule_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Rules_vertex_7_generator::mparse_parse_Rules_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Rules_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_92(stack_)};
            return std::make_unique<mparse_parse_Rules_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_0_generator::mparse_parse_Spec_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbols_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_1_generator::mparse_parse_Spec_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_2_generator::mparse_parse_Spec_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbols_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_3_generator::mparse_parse_Spec_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_4_generator::mparse_parse_Spec_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_95(stack_)};
            return std::make_unique<mparse_parse_Spec_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_5_generator::mparse_parse_Spec_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_6_generator::mparse_parse_Spec_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_96(stack_)};
            return std::make_unique<mparse_parse_Spec_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_7_generator::mparse_parse_Spec_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_8_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbols_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_8_generator::mparse_parse_Spec_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Spec_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Spec_vertex_9_generator::mparse_parse_Spec_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Spec_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_95(stack_)};
            return std::make_unique<mparse_parse_Spec_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContent_vertex_0_generator::mparse_parse_SquareContent_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContent_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContentBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_97(stack_)};
            return std::make_unique<mparse_parse_SquareContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContent_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContentAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContent_vertex_1_generator::mparse_parse_SquareContent_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SquareContent_vertex_2_generator::mparse_parse_SquareContent_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_98(stack_)};
            return std::make_unique<mparse_parse_SquareContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContent_vertex_3_generator::mparse_parse_SquareContent_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContent_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_99(stack_)};
            return std::make_unique<mparse_parse_SquareContent_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquarePlainChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Literal_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            const std::string_view literal = "[";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_100(stack_)};
            return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_101(stack_)};
            return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "]";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator::mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentAfterEmptyPart_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_102(stack_)};
            return std::make_unique<mparse_parse_SquareContentAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator::mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator::mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquareContentBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_0_generator::mparse_parse_SquarePlainChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 6) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquarePlainChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquarePlainChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            if (!(position_ < input_.size() && input_[position_] >= '!' && input_[position_] <= '&')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_4_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 3: {
            if (!(position_ < input_.size() && input_[position_] >= '(' && input_[position_] <= 'Z')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_5_generator>(input_, position_ + 1, std::move(next_stack));
        }
        case 4: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SquarePlainChar_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Backslash_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 5: {
            if (!(position_ < input_.size() && input_[position_] >= '^' && input_[position_] <= '~')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_7_generator>(input_, position_ + 1, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_1_generator::mparse_parse_SquarePlainChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_2_generator::mparse_parse_SquarePlainChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_103(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_3_generator::mparse_parse_SquarePlainChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_104(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_4_generator::mparse_parse_SquarePlainChar_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_105(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_5_generator::mparse_parse_SquarePlainChar_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_106(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_6_generator::mparse_parse_SquarePlainChar_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_107(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SquarePlainChar_vertex_7_generator::mparse_parse_SquarePlainChar_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SquarePlainChar_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_108(stack_)};
            return std::make_unique<mparse_parse_SquarePlainChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_0_generator::mparse_parse_Symbol_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 4) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_14_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 3: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_22_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_1_generator::mparse_parse_Symbol_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_2_generator::mparse_parse_Symbol_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_3_generator::mparse_parse_Symbol_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_4_generator::mparse_parse_Symbol_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rules_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_5_generator::mparse_parse_Symbol_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_109(stack_)};
            return std::make_unique<mparse_parse_Symbol_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_6_generator::mparse_parse_Symbol_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_7_generator::mparse_parse_Symbol_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "[";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_8_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_8_generator::mparse_parse_Symbol_vertex_8_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_8_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_8_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_9_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_9_generator::mparse_parse_Symbol_vertex_9_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_9_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_9_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "]";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_10_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_10_generator::mparse_parse_Symbol_vertex_10_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_10_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_10_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_11_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_11_generator::mparse_parse_Symbol_vertex_11_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_11_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_11_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_12_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_12_generator::mparse_parse_Symbol_vertex_12_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_12_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_12_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_13_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rules_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_13_generator::mparse_parse_Symbol_vertex_13_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_13_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_13_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_110(stack_)};
            return std::make_unique<mparse_parse_Symbol_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_14_generator::mparse_parse_Symbol_vertex_14_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_14_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_14_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_15_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_15_generator::mparse_parse_Symbol_vertex_15_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_15_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_15_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "<";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_16_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_16_generator::mparse_parse_Symbol_vertex_16_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_16_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_16_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_17_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_17_generator::mparse_parse_Symbol_vertex_17_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_17_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_17_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ">";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_18_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_18_generator::mparse_parse_Symbol_vertex_18_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_18_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_18_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_19_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_19_generator::mparse_parse_Symbol_vertex_19_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_19_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_19_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_20_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_20_generator::mparse_parse_Symbol_vertex_20_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_20_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_20_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_21_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rules_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_21_generator::mparse_parse_Symbol_vertex_21_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_21_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_21_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_111(stack_)};
            return std::make_unique<mparse_parse_Symbol_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_22_generator::mparse_parse_Symbol_vertex_22_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_22_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_22_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_23_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_23_generator::mparse_parse_Symbol_vertex_23_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_23_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_23_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "[";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_24_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_24_generator::mparse_parse_Symbol_vertex_24_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_24_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_24_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_25_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_25_generator::mparse_parse_Symbol_vertex_25_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_25_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_25_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "]";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_26_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_26_generator::mparse_parse_Symbol_vertex_26_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_26_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_26_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_27_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_27_generator::mparse_parse_Symbol_vertex_27_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_27_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_27_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "<";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_28_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_28_generator::mparse_parse_Symbol_vertex_28_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_28_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_28_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_29_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_AngleContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_29_generator::mparse_parse_Symbol_vertex_29_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_29_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_29_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = ">";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_Symbol_vertex_30_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_30_generator::mparse_parse_Symbol_vertex_30_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_30_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_30_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_31_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_31_generator::mparse_parse_Symbol_vertex_31_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_31_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_31_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_32_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_32_generator::mparse_parse_Symbol_vertex_32_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_32_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_32_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbol_vertex_33_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Rules_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbol_vertex_33_generator::mparse_parse_Symbol_vertex_33_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_33_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbol_vertex_33_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_112(stack_)};
            return std::make_unique<mparse_parse_Symbol_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_0_generator::mparse_parse_SymbolReference_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SymbolReference_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SymbolReference_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Identifier_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_1_generator::mparse_parse_SymbolReference_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_2_generator::mparse_parse_SymbolReference_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_113(stack_)};
            return std::make_unique<mparse_parse_SymbolReference_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_3_generator::mparse_parse_SymbolReference_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SymbolReference_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalWhitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_4_generator::mparse_parse_SymbolReference_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "[";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_SymbolReference_vertex_5_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_5_generator::mparse_parse_SymbolReference_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_SymbolReference_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_SquareContent_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_6_generator::mparse_parse_SymbolReference_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            const std::string_view literal = "]";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return std::make_unique<mparse_parse_SymbolReference_vertex_7_generator>(input_, position_ + literal.size(), std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_SymbolReference_vertex_7_generator::mparse_parse_SymbolReference_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_SymbolReference_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_114(stack_)};
            return std::make_unique<mparse_parse_SymbolReference_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_0_generator::mparse_parse_Symbols_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbol_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_1_generator::mparse_parse_Symbols_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_5_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbol_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_6_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_2_generator::mparse_parse_Symbols_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_115(stack_)};
            return std::make_unique<mparse_parse_Symbols_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_3_generator::mparse_parse_Symbols_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_4_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbol_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_4_generator::mparse_parse_Symbols_vertex_4_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_4_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_4_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_116(stack_)};
            return std::make_unique<mparse_parse_Symbols_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_5_generator::mparse_parse_Symbols_vertex_5_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_5_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_5_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_117(stack_)};
            return std::make_unique<mparse_parse_Symbols_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_6_generator::mparse_parse_Symbols_vertex_6_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_6_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_6_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Symbols_vertex_7_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Symbol_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Symbols_vertex_7_generator::mparse_parse_Symbols_vertex_7_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_7_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Symbols_vertex_7_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_116(stack_)};
            return std::make_unique<mparse_parse_Symbols_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Whitespace_vertex_0_generator::mparse_parse_Whitespace_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 3) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Whitespace_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceBeforeEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            std::vector<std::any> next_stack{mparse_action_94(stack_)};
            return std::make_unique<mparse_parse_Whitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        case 2: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_Whitespace_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceAfterEmptyPart_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_Whitespace_vertex_1_generator::mparse_parse_Whitespace_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_Whitespace_vertex_2_generator::mparse_parse_Whitespace_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_118(stack_)};
            return std::make_unique<mparse_parse_Whitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_Whitespace_vertex_3_generator::mparse_parse_Whitespace_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_Whitespace_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_119(stack_)};
            return std::make_unique<mparse_parse_Whitespace_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator::mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_WhitespaceChar_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator::mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator::mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_Whitespace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator::mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceAfterEmptyPart_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_120(stack_)};
            return std::make_unique<mparse_parse_WhitespaceAfterEmptyPart_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator::mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator::makeNextGenerator() {
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceBeforeEmptyPart_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator::mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceBeforeEmptyPart_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceChar_vertex_0_generator::mparse_parse_WhitespaceChar_vertex_0_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_0_generator::makeNextGenerator() {
    while (edge_index_ < 2) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_0_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_WhitespaceChar_vertex_2_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_HorizontalSpace_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        case 1: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack)
                    : input(input)
                    , stack(std::move(stack))
                    , nested_generator(input, position)
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return std::make_unique<mparse_parse_WhitespaceChar_vertex_3_generator>(input, nested->position, std::move(next_stack));
                }

                std::string_view input;
                std::vector<std::any> stack;
                mparse_parse_LineBreak_generator nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_);
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceChar_vertex_1_generator::mparse_parse_WhitespaceChar_vertex_1_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(!stack_.empty())
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_1_generator::makeNextGenerator() {
    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_1_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceChar_vertex_2_generator::mparse_parse_WhitespaceChar_vertex_2_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_2_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_2_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_121(stack_)};
            return std::make_unique<mparse_parse_WhitespaceChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

mparse_parse_WhitespaceChar_vertex_3_generator::mparse_parse_WhitespaceChar_vertex_3_generator(std::string_view input, size_t position, std::vector<std::any> stack)
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
    , terminal_pending_(false)
    {}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_3_generator::makeNextGenerator() {
    while (edge_index_ < 1) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
    return nullptr;
}

std::unique_ptr<mparse_generated_detail::PartialGenerator> mparse_parse_WhitespaceChar_vertex_3_generator::makeEdge(size_t edge_index) {
    switch (edge_index) {
        case 0: {
            std::vector<std::any> next_stack{mparse_action_122(stack_)};
            return std::make_unique<mparse_parse_WhitespaceChar_vertex_1_generator>(input_, position_, std::move(next_stack));
        }
        default:
            return nullptr;
    }
}

std::optional<std::vector<mparse::spec::Symbol>> parse(std::string_view input) {
    auto generator = mparse_parse_Spec_generator(input, 0);
    while (auto result = generator.next()) {
        if (result->position == input.size()) {
            return result->value;
        }
    }
    return std::nullopt;
}
// mparse: end grammar

namespace mparse::spec::generated {

    std::optional<std::vector<mparse::spec::Symbol>> parseSymbols(std::string_view source) {
        return ::parse(source);
    }

} // namespace mparse::spec::generated
