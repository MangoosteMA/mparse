#include "utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mparse::spec {

    namespace {

        constexpr std::string_view kBeginMarker = "// mparse: begin grammar";
        constexpr std::string_view kEndMarker = "// mparse: end grammar";

        size_t lineEndOffset(std::string_view source, size_t line_start) {
            const size_t line_end = source.find('\n', line_start);
            if (line_end == std::string_view::npos) {
                return source.size();
            }
            return line_end + 1;
        }

    } // namespace

    ParseError::ParseError(size_t line, size_t column, const std::string& message)
        : std::runtime_error("spec parse error at " + std::to_string(line) + ":" + std::to_string(column) + ": " + message) {}

    std::string trim(std::string_view value) {
        size_t begin = 0;
        while (begin < value.size() &&
               std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
            begin++;
        }

        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            end--;
        }

        return std::string{value.substr(begin, end - begin)};
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream file{path};
        if (!file.is_open()) {
            throw std::runtime_error("failed to open specification file: " + path.string());
        }

        std::ostringstream source;
        source << file.rdbuf();
        return source.str();
    }

    ExtractedGrammarBlock extractGrammarBlock(std::string source) {
        const size_t begin_marker = source.find(kBeginMarker);
        if (begin_marker == std::string::npos) {
            throw std::runtime_error("grammar begin marker was not found");
        }

        const size_t begin_line_end = lineEndOffset(source, begin_marker);
        const size_t end_marker = source.find(kEndMarker, begin_line_end);
        if (end_marker == std::string::npos) {
            throw std::runtime_error("grammar end marker was not found");
        }

        return ExtractedGrammarBlock{
            .grammar = source.substr(begin_line_end, end_marker - begin_line_end),
            .first_line =
                static_cast<size_t>(
                    std::count(source.begin(), source.begin() + begin_line_end, '\n')
                ) +
                1,
            .source_template = SourceTemplate{
                .before_insertion = source.substr(0, begin_line_end),
                .after_insertion = source.substr(end_marker),
            },
        };
    }

} // namespace mparse::spec
