R"mparse_template(std::optional<{{ value_type }}> parse(std::string_view input{{ parameters_declaration }}) {
    auto generator = {{ generator_name }}(input, 0{{ parameters_forwarding }});
    while (auto result = generator.next()) {
        if (result->position == input.size()) {
            return result->value;
        }
    }
    return std::nullopt;
}
)mparse_template"
