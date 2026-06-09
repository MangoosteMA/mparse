#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>

namespace mparse::codegen::cpp {

    std::string renderTemplate(std::string_view template_source, const nlohmann::json& data);

} // namespace mparse::codegen::cpp
