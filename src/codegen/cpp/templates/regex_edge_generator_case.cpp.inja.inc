R"mparse_template(        case {{ edge_index }}: {
{{ matcher_functions }}            const auto regex_match_positions = {{ root_matcher }}({{ root_matcher }}, position_);
            if (regex_match_positions.empty()) {
                return nullptr;
            }

            class RegexEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                RegexEdgeGenerator(
                    std::string_view input,
                    size_t position,
                    std::vector<std::any> stack,
                    std::vector<size_t> match_positions
                )
                    : input(input)
                    , position(position)
                    , stack(std::move(stack))
                    , match_positions(std::move(match_positions))
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    if (match_index >= match_positions.size()) {
                        return nullptr;
                    }

                    const auto match_position = match_positions[match_index++];
                    auto next_stack = stack;
                    next_stack.push_back(std::string{input.substr(position, match_position - position)});
                    return {{ next_generator }};
                }

                std::string_view input;
                size_t position = 0;
                std::vector<std::any> stack;
                std::vector<size_t> match_positions;
                size_t match_index = 0;
            };

            return std::make_unique<RegexEdgeGenerator>(
                input_,
                position_,
                stack_,
                regex_match_positions
            );
        }
)mparse_template"
