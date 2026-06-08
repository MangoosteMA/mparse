#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace mparse::codegen::cpp {

    class Writer {
    public:
        void indent();
        void unindent();
        void line(std::string_view text = {});
        std::string str() const;

    private:
        size_t indentation_ = 0;
        std::ostringstream output_;
    };

} // namespace mparse::codegen::cpp
