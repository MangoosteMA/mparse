R"mparse_template(        case {{ edge_index }}: {
            const std::string_view literal = "{{ literal }}";
            const auto repeat_count = ({{ count_expression }});
            if (std::cmp_less(repeat_count, 0) || position_ > input_.size()) {
                return nullptr;
            }

            const auto repeat_count_size = static_cast<size_t>(repeat_count);
            const auto available_size = input_.size() - position_;
            if (!literal.empty() && repeat_count_size > available_size / literal.size()) {
                return nullptr;
            }

            const auto repeated_size = literal.size() * repeat_count_size;
            if (!literal.empty()) {
                for (size_t offset = 0; offset < repeated_size; offset += literal.size()) {
                    if (input_.substr(position_ + offset, literal.size()) != literal) {
                        return nullptr;
                    }
                }
            }

            auto next_stack = stack_;
            next_stack.push_back(std::string{input_.substr(position_, repeated_size)});
            return {{ next_generator }};
        }
)mparse_template"
