R"mparse_template({{ generator_name }}::{{ generator_name }}(std::string_view input, size_t position{{ parameters_declaration }})
    : partial_generator_({{ start_generator }}) {}

std::optional<{{ generator_name }}::Result> {{ generator_name }}::next() {
    while (auto partial = partial_generator_->next()) {
        if (partial->stack.empty()) {
            continue;
        }
        return Result{
            .position = partial->position,
            .value = std::any_cast<{{ value_type }}>(partial->stack.back()),
        };
    }
    return std::nullopt;
}

)mparse_template"
