#pragma once

#include "data.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mparse::spec {

    class Parser {
    public:
        Parser(std::string_view source, size_t first_line = 1);

        std::vector<Symbol> parseSymbols();

    private:
        bool eof() const;
        char peek() const;
        char consume();
        bool consumeIf(char expected);
        void expect(char expected);

        [[noreturn]] void fail(const std::string& message) const;

        void skipWhitespace();
        void skipHorizontalSpaces();

        bool isIdentifierStart(char value) const;
        bool isIdentifierPart(char value) const;

        std::string parseIdentifier();
        std::string parseBalanced(char open, char close);
        std::vector<std::string> parseCommaSeparatedExpressions(char open, char close);

        Symbol parseSymbol();
        std::vector<SymbolArgument> parseSymbolArguments();
        Rule parseRule();
        RuleItem parseRuleItem();
        RuleItemValue parseLiteralOrRange();
        std::string parseRepeatCountExpression();
        std::string parseLiteralText();
        char parseEscapedLiteralChar(char value) const;
        RuleItemSymbol parseSymbolReference();

        std::string_view source_;
        size_t position_ = 0;
        size_t line_ = 1;
        size_t column_ = 1;
    };

} // namespace mparse::spec
