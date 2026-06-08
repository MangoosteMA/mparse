#include <cstdint>
#include <iostream>

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

// mparse: begin grammar

HexColor<Color>
    : '#' HexByte HexByte HexByte {
        $$ = Color{
            .r = static_cast<std::uint8_t>($2),
            .g = static_cast<std::uint8_t>($3),
            .b = static_cast<std::uint8_t>($4),
        }
    }

HexByte<int>
    : HexDigit HexDigit { $$ = $1 * 16 + $2 }

HexDigit<int>
    : '0'-'9' { $$ = $1 - '0' }
    : 'a'-'f' { $$ = $1 - 'a' + 10 }
    : 'A'-'F' { $$ = $1 - 'A' + 10 }

// mparse: end grammar

int main() {
    const auto color = parse("#1aF09C");
    if (!color) {
        return 1;
    }

    std::cout
        << static_cast<int>(color->r) << ' '
        << static_cast<int>(color->g) << ' '
        << static_cast<int>(color->b) << '\n';
    return 0;
}
