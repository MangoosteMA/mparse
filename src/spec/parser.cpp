#include "parser.h"

#include "utils.h"

#include <cctype>
#include <memory>
#include <utility>
#include <variant>

namespace mparse::spec {

    Parser::Parser(std::string_view source, size_t first_line)
        : source_{source}
        , line_{first_line} {}

    std::vector<Symbol> Parser::parseSymbols() {
        std::vector<Symbol> symbols;

        skipWhitespace();
        while (!eof()) {
            symbols.push_back(parseSymbol());
            skipWhitespace();
        }

        return symbols;
    }

    bool Parser::eof() const {
        return position_ >= source_.size();
    }

    char Parser::peek() const {
        if (eof()) {
            return '\0';
        }
        return source_[position_];
    }

    char Parser::consume() {
        const char current = peek();
        if (current == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        position_++;
        return current;
    }

    bool Parser::consumeIf(char expected) {
        if (peek() != expected) {
            return false;
        }
        consume();
        return true;
    }

    void Parser::expect(char expected) {
        if (!consumeIf(expected)) {
            fail(std::string{"expected '"} + expected + "'");
        }
    }

    [[noreturn]] void Parser::fail(const std::string& message) const {
        throw ParseError{line_, column_, message};
    }

    void Parser::skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            consume();
        }
    }

    void Parser::skipHorizontalSpaces() {
        while (!eof() && peek() != '\n' &&
               std::isspace(static_cast<unsigned char>(peek()))) {
            consume();
        }
    }

    bool Parser::isIdentifierStart(char value) const {
        return std::isalpha(static_cast<unsigned char>(value)) || value == '_';
    }

    bool Parser::isIdentifierPart(char value) const {
        return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
    }

    std::string Parser::parseIdentifier() {
        if (!isIdentifierStart(peek())) {
            fail("expected identifier");
        }

        const size_t begin = position_;
        consume();
        while (!eof() && isIdentifierPart(peek())) {
            consume();
        }
        return std::string{source_.substr(begin, position_ - begin)};
    }

    std::string Parser::parseBalanced(char open, char close) {
        expect(open);

        const size_t content_begin = position_;
        size_t depth = 1;
        bool inside_literal = false;
        bool escaped = false;

        while (!eof()) {
            const char current = consume();

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
            } else if (current == open) {
                depth++;
            } else if (current == close) {
                depth--;
                if (depth == 0) {
                    const size_t content_end = position_ - 1;
                    return trim(source_.substr(content_begin, content_end - content_begin));
                }
            }
        }

        fail(std::string{"expected '"} + close + "'");
    }

    std::vector<std::string> Parser::parseCommaSeparatedExpressions(char open, char close) {
        const std::string content = parseBalanced(open, close);
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
                result.push_back(trim(std::string_view{content}.substr(begin, index - begin)));
                begin = index + 1;
            }
        }

        const std::string last = trim(std::string_view{content}.substr(begin));
        if (!last.empty()) {
            result.push_back(last);
        }
        return result;
    }

    Symbol Parser::parseSymbol() {
        Symbol symbol;
        symbol.source_reference = SourceReference{
            .line = line_,
            .column = column_,
        };
        symbol.name = parseIdentifier();

        skipHorizontalSpaces();

        if (peek() == '[') {
            symbol.arguments = parseSymbolArguments();
            skipHorizontalSpaces();
        }

        if (peek() == '<') {
            symbol.type = parseBalanced('<', '>');
            skipHorizontalSpaces();
        }

        skipWhitespace();
        while (peek() == ':') {
            symbol.rules.push_back(parseRule());
            skipWhitespace();
        }

        if (symbol.rules.empty()) {
            fail("expected at least one rule after symbol declaration");
        }

        return symbol;
    }

    std::vector<SymbolArgument> Parser::parseSymbolArguments() {
        const auto raw_arguments = parseCommaSeparatedExpressions('[', ']');
        std::vector<SymbolArgument> arguments;
        arguments.reserve(raw_arguments.size());

        for (const auto& raw_argument : raw_arguments) {
            const size_t split = raw_argument.find_last_of(" \t");
            if (split == std::string::npos) {
                fail("symbol argument must have name and type: " + raw_argument);
            }

            arguments.push_back(SymbolArgument{
                .name = trim(std::string_view{raw_argument}.substr(0, split)),
                .type = trim(std::string_view{raw_argument}.substr(split + 1)),
            });
        }

        return arguments;
    }

    Rule Parser::parseRule() {
        expect(':');
        Rule rule;

        skipHorizontalSpaces();
        while (!eof() && peek() != '\n' && peek() != '{') {
            rule.items.push_back(parseRuleItem());
            skipHorizontalSpaces();
        }

        if (peek() == '{') {
            rule.action = parseBalanced('{', '}');
        }

        return rule;
    }

    RuleItem Parser::parseRuleItem() {
        if (peek() == '(') {
            return RuleItem{.value = parseParenthesizedRegexRuleItem()};
        }

        if (peek() == '\'') {
            auto item = parseLiteralOrRange();
            if (const auto repeat_kind = consumeRegexRepeatKind()) {
                return RuleItem{.value = makePostfixRegexRuleItem(
                    std::move(item),
                    *repeat_kind
                )};
            }
            return RuleItem{.value = std::move(item)};
        }

        return RuleItem{.value = parseSymbolReference()};
    }

    RuleItemValue Parser::parseLiteralOrRange() {
        const auto first_literal = parseLiteralText();
        skipHorizontalSpaces();

        if (!consumeIf('-')) {
            if (consumeIf('^')) {
                return RuleItemRepeatedLiteral{
                    .value = first_literal,
                    .count_expression = parseRepeatCountExpression(),
                };
            }
            return RuleItemLiteral{.value = first_literal};
        }

        skipHorizontalSpaces();
        const auto second_literal = parseLiteralText();
        if (first_literal.size() != 1 || second_literal.size() != 1) {
            fail("range boundaries must be one-character literals");
        }

        return RuleItemRange{
            .from = first_literal.front(),
            .to = second_literal.front(),
        };
    }

    std::string Parser::parseRepeatCountExpression() {
        skipHorizontalSpaces();

        if (peek() == '(') {
            return parseBalanced('(', ')');
        }

        const size_t begin = position_;
        size_t depth = 0;
        bool inside_literal = false;
        bool escaped = false;

        while (!eof() && peek() != '\n' && peek() != '{') {
            const char current = peek();

            if (inside_literal) {
                consume();
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
                if (depth == 0) {
                    break;
                }
                depth--;
            } else if (std::isspace(static_cast<unsigned char>(current)) && depth == 0) {
                break;
            }

            consume();
        }

        auto result = trim(source_.substr(begin, position_ - begin));
        if (result.empty()) {
            fail("expected repeat count expression after '^'");
        }
        return result;
    }

    namespace {

        RegexExpressionPtr makeRegexExpressionPtr(RegexExpression expression) {
            return std::make_shared<RegexExpression>(std::move(expression));
        }

    } // namespace

    RuleItemRegex Parser::parseParenthesizedRegexRuleItem() {
        auto expression = parseRegexAtom();
        if (const auto repeat_kind = consumeRegexRepeatKind()) {
            expression = RegexExpression{
                .value = RegexRepeat{
                    .item = makeRegexExpressionPtr(std::move(expression)),
                    .kind = *repeat_kind,
                },
            };
        }

        return RuleItemRegex{
            .expression = std::move(expression),
        };
    }

    RuleItemRegex Parser::makePostfixRegexRuleItem(
        RuleItemValue item,
        RegexRepeatKind repeat_kind
    ) {
        RegexExpression expression;
        if (auto* literal = std::get_if<RuleItemLiteral>(&item)) {
            expression = RegexExpression{
                .value = RegexLiteral{.value = std::move(literal->value)},
            };
        } else if (auto* range = std::get_if<RuleItemRange>(&item)) {
            expression = RegexExpression{
                .value = RegexRange{
                    .from = range->from,
                    .to = range->to,
                },
            };
        } else {
            fail("only literal and range items can use regex postfix operators");
        }

        return RuleItemRegex{
            .expression = RegexExpression{
                .value = RegexRepeat{
                    .item = makeRegexExpressionPtr(std::move(expression)),
                    .kind = repeat_kind,
                },
            },
        };
    }

    RegexExpression Parser::parseRegexAlternative() {
        std::vector<RegexExpressionPtr> alternatives;
        alternatives.push_back(makeRegexExpressionPtr(parseRegexSequence()));

        skipHorizontalSpaces();
        while (consumeIf('|')) {
            skipHorizontalSpaces();
            alternatives.push_back(makeRegexExpressionPtr(parseRegexSequence()));
            skipHorizontalSpaces();
        }

        if (alternatives.size() == 1) {
            return std::move(*alternatives.front());
        }

        return RegexExpression{
            .value = RegexAlternative{
                .alternatives = std::move(alternatives),
            },
        };
    }

    RegexExpression Parser::parseRegexSequence() {
        std::vector<RegexExpressionPtr> items;

        skipHorizontalSpaces();
        while (peek() == '\'' || peek() == '(') {
            items.push_back(makeRegexExpressionPtr(parseRegexRepeat()));
            skipHorizontalSpaces();
        }

        if (items.empty()) {
            fail("expected regex expression");
        }

        if (items.size() == 1) {
            return std::move(*items.front());
        }

        return RegexExpression{
            .value = RegexSequence{
                .items = std::move(items),
            },
        };
    }

    RegexExpression Parser::parseRegexRepeat() {
        auto expression = parseRegexAtom();

        skipHorizontalSpaces();
        if (const auto repeat_kind = consumeRegexRepeatKind()) {
            return RegexExpression{
                .value = RegexRepeat{
                    .item = makeRegexExpressionPtr(std::move(expression)),
                    .kind = *repeat_kind,
                },
            };
        }

        return expression;
    }

    RegexExpression Parser::parseRegexAtom() {
        if (peek() == '\'') {
            return parseRegexLiteralOrRange();
        }

        expect('(');
        skipHorizontalSpaces();
        auto expression = parseRegexAlternative();
        skipHorizontalSpaces();
        expect(')');
        return expression;
    }

    RegexExpression Parser::parseRegexLiteralOrRange() {
        const auto first_literal = parseLiteralText();
        skipHorizontalSpaces();

        if (!consumeIf('-')) {
            return RegexExpression{
                .value = RegexLiteral{.value = first_literal},
            };
        }

        skipHorizontalSpaces();
        const auto second_literal = parseLiteralText();
        if (first_literal.size() != 1 || second_literal.size() != 1) {
            fail("range boundaries must be one-character literals");
        }

        return RegexExpression{
            .value = RegexRange{
                .from = first_literal.front(),
                .to = second_literal.front(),
            },
        };
    }

    std::optional<RegexRepeatKind> Parser::consumeRegexRepeatKind() {
        if (consumeIf('*')) {
            return RegexRepeatKind::ZeroOrMore;
        }
        if (consumeIf('+')) {
            return RegexRepeatKind::OneOrMore;
        }
        return std::nullopt;
    }

    std::string Parser::parseLiteralText() {
        expect('\'');

        std::string result;
        bool escaped = false;
        while (!eof()) {
            const char current = consume();

            if (escaped) {
                result.push_back(parseEscapedLiteralChar(current));
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '\'') {
                return result;
            } else {
                result.push_back(current);
            }
        }

        fail("expected closing literal quote");
    }

    char Parser::parseEscapedLiteralChar(char value) const {
        switch (value) {
            case 'n':
                return '\n';
            case 't':
                return '\t';
            case '\\':
            case '\'':
                return value;
            default:
                return value;
        }
    }

    RuleItemSymbol Parser::parseSymbolReference() {
        RuleItemSymbol symbol;
        symbol.name = parseIdentifier();
        skipHorizontalSpaces();

        if (peek() == '[') {
            symbol.arguments = parseCommaSeparatedExpressions('[', ']');
        }

        return symbol;
    }

} // namespace mparse::spec
