R"mparse_template(        case {{ edge_index }}: {
            if (!(position_ < input_.size() && input_[position_] >= '{{ from }}' && input_[position_] <= '{{ to }}')) {
                return nullptr;
            }
            auto next_stack = stack_;
            next_stack.push_back(input_[position_]);
            return {{ next_generator }};
        }
)mparse_template"
