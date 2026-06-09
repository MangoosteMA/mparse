R"mparse_template(class {{ generator_name }} : public mparse_generated_detail::SequentialPartialGenerator {
public:
    {{ generator_name }}(std::string_view input, size_t position, std::vector<std::any> stack{{ parameters_declaration }});

private:
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override;
    std::unique_ptr<mparse_generated_detail::PartialGenerator> makeEdge(size_t edge_index);

    std::string_view input_;
    size_t position_ = 0;
    std::vector<std::any> stack_;
{{ parameter_fields }}    bool terminal_pending_ = false;
    size_t edge_index_ = 0;
};

)mparse_template"
