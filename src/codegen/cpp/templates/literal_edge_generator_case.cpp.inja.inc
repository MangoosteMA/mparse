R"mparse_template(        case {{ edge_index }}: {
            const std::string_view literal = "{{ literal }}";
            if (position_ > input_.size() || input_.substr(position_, literal.size()) != literal) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(std::string{literal});
            return {{ next_generator }};
        }
)mparse_template"
