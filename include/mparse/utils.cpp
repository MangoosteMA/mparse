#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace mparse {

    [[noreturn]] void exitWithError(const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        std::exit(1);
    }

    [[noreturn]] void failVerification(
        std::string_view expression,
        const std::source_location& location
    ) {
        throw std::runtime_error(
            "verification failed: " + std::string{expression} + "\n  at " +
            location.file_name() + std::string{":"} +
            std::to_string(location.line()) + "\n  in " +
            location.function_name()
        );
    }

} // namespace mparse
