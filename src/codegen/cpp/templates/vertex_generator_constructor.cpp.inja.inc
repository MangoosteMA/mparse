R"mparse_template({{ generator_name }}::{{ generator_name }}(std::string_view input, size_t position, std::vector<std::any> stack{{ parameters_declaration }})
    : input_(input)
    , position_(position)
    , stack_(std::move(stack))
{{ parameter_initializers }}    , terminal_pending_({{ terminal_pending }})
    {}

)mparse_template"
