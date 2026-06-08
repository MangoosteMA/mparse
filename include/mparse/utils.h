#pragma once

#include <exception>
#include <source_location>
#include <string_view>
#include <utility>

#define VERIFY(expression) \
    ::mparse::verify((expression), #expression, std::source_location::current())

namespace mparse {

    template <typename... Functions>
    struct Overloaded : Functions... {
        using Functions::operator()...;

        consteval void operator()(auto) const {
            static_assert(false, "Unsupported type");
        }
    };

    [[noreturn]] void exitWithError(const std::exception& error);

    [[noreturn]] void failVerification(
        std::string_view expression,
        const std::source_location& location
    );

    template <typename Value>
    decltype(auto) verify(
        Value&& value,
        std::string_view expression,
        const std::source_location& location = std::source_location::current()
    ) {
        if (!value) {
            failVerification(expression, location);
        }
        return std::forward<Value>(value);
    }

} // namespace mparse
