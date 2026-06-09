R"mparse_template(        case {{ edge_index }}: {
            class SymbolEdgeGenerator : public mparse_generated_detail::SequentialPartialGenerator {
            public:
                SymbolEdgeGenerator(std::string_view input, size_t position, std::vector<std::any> stack{{ parameters_declaration }})
                    : input(input)
                    , stack(std::move(stack))
{{ parameter_initializers }}                    , nested_generator(input, position{{ nested_arguments }})
                    {}

            private:
                std::unique_ptr<mparse_generated_detail::PartialGenerator> makeNextGenerator() override {
                    auto nested = nested_generator.next();
                    if (!nested) {
                        return nullptr;
                    }
                    auto next_stack = stack;
                    next_stack.push_back(nested->value);
                    return {{ next_generator }};
                }

                std::string_view input;
                std::vector<std::any> stack;
{{ parameter_fields }}                {{ nested_generator_name }} nested_generator;
            };
            return std::make_unique<SymbolEdgeGenerator>(input_, position_, stack_{{ parameters_forwarding }});
        }
)mparse_template"
