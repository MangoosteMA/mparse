#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mparse::tests {

    std::string canonicalizeText(std::string_view text);

    std::string readOrUpdateCanon(
        const std::filesystem::path& relative_path,
        std::string_view actual
    );

} // namespace mparse::tests
