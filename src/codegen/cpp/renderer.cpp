#include "renderer.h"

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace mparse::codegen::cpp {

    std::string renderTemplate(std::string_view template_source, const nlohmann::json& data) {
        inja::Environment environment;
        return environment.render(std::string{template_source}, data);
    }

} // namespace mparse::codegen::cpp
