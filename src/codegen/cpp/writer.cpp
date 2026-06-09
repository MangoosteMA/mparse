#include "writer.h"

#include <mparse/utils.h>

namespace mparse::codegen::cpp {

    void Writer::indent() {
        indentation_++;
    }

    void Writer::unindent() {
        VERIFY(indentation_ > 0);
        indentation_--;
    }

    void Writer::line(std::string_view text) {
        for (size_t index = 0; index < indentation_; index++) {
            output_ << "    ";
        }
        output_ << text << '\n';
    }

    void Writer::write(std::string_view text) {
        output_ << text;
    }

    std::string Writer::str() const {
        return output_.str();
    }

} // namespace mparse::codegen::cpp
