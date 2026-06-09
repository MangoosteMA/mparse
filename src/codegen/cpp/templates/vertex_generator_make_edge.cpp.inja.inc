R"mparse_template(std::unique_ptr<mparse_generated_detail::PartialGenerator> {{ generator_name }}::makeEdge(size_t edge_index) {
    switch (edge_index) {
{{ cases }}        default:
            return nullptr;
    }
}

)mparse_template"
