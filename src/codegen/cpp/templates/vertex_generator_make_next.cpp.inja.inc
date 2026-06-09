R"mparse_template(std::unique_ptr<mparse_generated_detail::PartialGenerator> {{ generator_name }}::makeNextGenerator() {
{% if has_terminal %}    if (terminal_pending_) {
        terminal_pending_ = false;
        return std::make_unique<mparse_generated_detail::SinglePartialGenerator>(
            mparse_generated_detail::PartialResult{
                .position = position_,
                .stack = stack_,
            }
        );
    }
{% endif %}{% if has_edges %}    while (edge_index_ < {{ edge_count }}) {
        auto generator = makeEdge(edge_index_++);
        if (generator) {
            return generator;
        }
    }
{% endif %}    return nullptr;
}

)mparse_template"
