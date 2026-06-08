#pragma once

#include "data.h"

#include <filesystem>

namespace mparse::spec {

    Specification readSpecificationOrThrow(const std::filesystem::path& path);
    Specification readSpecification(const std::filesystem::path& path) noexcept;

} // namespace mparse::spec
