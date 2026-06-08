#include "canon.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace mparse::tests {

    namespace {

        std::filesystem::path canonPath(const std::filesystem::path& relative_path) {
            return std::filesystem::path{kMparseTestCanonsDir} / relative_path;
        }

        bool shouldUpdateCanons() {
            const auto* value = std::getenv("MPARSE_UPDATE_CANONS");
            return value != nullptr && std::string_view{value} != "" &&
                   std::string_view{value} != "0" &&
                   std::string_view{value} != "false" &&
                   std::string_view{value} != "FALSE";
        }

        std::string readFile(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("failed to open canon file: " + path.string());
            }

            return std::string{
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{},
            };
        }

        void writeFile(const std::filesystem::path& path, std::string_view content) {
            std::filesystem::create_directories(path.parent_path());

            std::ofstream output(path, std::ios::binary);
            if (!output) {
                throw std::runtime_error("failed to write canon file: " + path.string());
            }

            output << content;
        }

        std::string normalizeLineEndings(std::string_view text) {
            std::string normalized;
            normalized.reserve(text.size());

            for (size_t index = 0; index < text.size(); index++) {
                if (text[index] != '\r') {
                    normalized.push_back(text[index]);
                    continue;
                }

                normalized.push_back('\n');
                if (index + 1 < text.size() && text[index + 1] == '\n') {
                    index++;
                }
            }

            return normalized;
        }

        void trimTrailingHorizontalSpaces(std::string& text) {
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
                text.pop_back();
            }
        }

    } // namespace

    std::string canonicalizeText(std::string_view text) {
        const auto normalized = normalizeLineEndings(text);

        std::string result;
        result.reserve(normalized.size());

        size_t line_begin = 0;
        while (line_begin < normalized.size()) {
            const auto line_end = normalized.find('\n', line_begin);
            auto line = normalized.substr(
                line_begin,
                line_end == std::string::npos ? std::string::npos : line_end - line_begin
            );
            trimTrailingHorizontalSpaces(line);
            result += line;

            if (line_end == std::string::npos) {
                break;
            }

            result.push_back('\n');
            line_begin = line_end + 1;
        }

        while (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }

        if (!result.empty()) {
            result.push_back('\n');
        }

        return result;
    }

    std::string readOrUpdateCanon(
        const std::filesystem::path& relative_path,
        std::string_view actual
    ) {
        const auto canonical_actual = canonicalizeText(actual);
        const auto path = canonPath(relative_path);

        if (shouldUpdateCanons()) {
            writeFile(path, canonical_actual);
            return canonical_actual;
        }

        return canonicalizeText(readFile(path));
    }

} // namespace mparse::tests
