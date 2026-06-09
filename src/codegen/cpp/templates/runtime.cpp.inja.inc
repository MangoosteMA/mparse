R"mparse_template(#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace mparse_generated_detail {
    template <typename Value>
    struct Result {
        size_t position = 0;
        Value value{};
    };

    struct PartialResult {
        size_t position = 0;
        std::vector<std::any> stack;
    };

    class PartialGenerator {
    public:
        virtual ~PartialGenerator() = default;
        virtual std::optional<PartialResult> next() = 0;
    };

    class SequentialPartialGenerator : public PartialGenerator {
    public:
        std::optional<PartialResult> next() override {
            while (true) {
                if (current_generator_) {
                    if (auto result = current_generator_->next()) {
                        return result;
                    }
                    current_generator_.reset();
                }
                current_generator_ = makeNextGenerator();
                if (!current_generator_) {
                    return std::nullopt;
                }
            }
        }

    protected:
        virtual std::unique_ptr<PartialGenerator> makeNextGenerator() = 0;

    private:
        std::unique_ptr<PartialGenerator> current_generator_;
    };

    class SinglePartialGenerator final : public PartialGenerator {
    public:
        explicit SinglePartialGenerator(PartialResult result)
            : result_(std::move(result)) {}

        std::optional<PartialResult> next() override {
            if (!result_) {
                return std::nullopt;
            }
            auto result = std::move(result_);
            result_.reset();
            return result;
        }

    private:
        std::optional<PartialResult> result_;
    };

    template <typename Value>
    const Value& semanticValue(const std::vector<std::any>& args, size_t index) {
        return std::any_cast<const Value&>(args.at(index));
    }

    inline void pushUnique(std::vector<size_t>& values, size_t value) {
        for (const auto current : values) {
            if (current == value) {
                return;
            }
        }
        values.push_back(value);
    }
} // namespace mparse_generated_detail

)mparse_template"
