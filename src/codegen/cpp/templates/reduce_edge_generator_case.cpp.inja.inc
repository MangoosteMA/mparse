R"mparse_template(        case {{ edge_index }}: {
            std::vector<std::any> next_stack{{ left_brace }}{{ action_name }}(stack_{{ parameters_forwarding }}){{ right_brace }};
            return {{ next_generator }};
        }
)mparse_template"
