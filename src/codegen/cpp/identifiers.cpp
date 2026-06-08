#include "identifiers.h"

#include <mparse/utils.h>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace mparse::codegen::cpp {

    namespace {

        bool isIdentifierCharacter(char value) {
            return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
                   value == '_';
        }

        std::string escapeByte(unsigned char value) {
            std::ostringstream output;
            output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<int>(value);
            return output.str();
        }

    } // namespace

    std::string cppIdentifier(std::string_view value) {
        std::string result;
        result.reserve(value.size() + 1);

        for (const char character : value) {
            if (isIdentifierCharacter(character)) {
                result.push_back(character);
            } else {
                result.push_back('_');
            }
        }

        if (result.empty() ||
            std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
            result.insert(result.begin(), '_');
        }

        return result;
    }

    std::string symbolBaseName(const analysis::SymbolPtr& symbol) {
        return cppIdentifier(VERIFY(symbol)->name());
    }

    std::string parseFunctionName(const analysis::SymbolPtr& symbol) {
        return "mparse_parse_" + symbolBaseName(symbol);
    }

    std::string vertexFunctionName(
        const analysis::SymbolPtr& symbol,
        size_t vertex_index
    ) {
        return parseFunctionName(symbol) + "_vertex_" + std::to_string(vertex_index);
    }

    std::string actionFunctionName(size_t action_index) {
        return "mparse_action_" + std::to_string(action_index);
    }

    std::string escapeStringLiteral(std::string_view value) {
        std::string result;
        result.reserve(value.size());

        for (const unsigned char character : value) {
            switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
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
                if (character < 0x20 || character >= 0x7f) {
                    result += escapeByte(character);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
            }
        }

        return result;
    }

    std::string escapeCharLiteral(char value) {
        switch (value) {
        case '\\':
            return "\\\\";
        case '\'':
            return "\\'";
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '\t':
            return "\\t";
        default:
            const auto unsigned_value = static_cast<unsigned char>(value);
            if (unsigned_value < 0x20 || unsigned_value >= 0x7f) {
                return escapeByte(unsigned_value);
            }
            return std::string{value};
        }
    }

} // namespace mparse::codegen::cpp
