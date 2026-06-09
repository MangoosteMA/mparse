R"mparse_template(class {{ generator_name }} {
public:
    using Result = mparse_generated_detail::Result<{{ value_type }}>;

    {{ generator_name }}(std::string_view input, size_t position{{ parameters_declaration }});
    std::optional<Result> next();

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> partial_generator_;
};

)mparse_template"
