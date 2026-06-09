#include <iostream>
#include <string>

// mparse: begin grammar

Outline[start_indent int]<std::string>
    : Item[start_indent] {
        $$ = $1
    }

Item[indent int]<std::string>
    : ' '^indent '- ' Label '\n' Item[indent + 2] {
        $$ = $3 + " > " + $5
    }
    : ' '^indent '- ' Label '\n' {
        $$ = $3
    }

Label<std::string>
    : 'project' {
        $$ = $1
    }
    : 'parser' {
        $$ = $1
    }
    : 'grammar' {
        $$ = $1
    }

// mparse: end grammar

int main() {
    const std::string outline = "- project\n  - parser\n    - grammar\n";
    const std::string shifted_outline = "    - parser\n      - grammar\n";

    const auto full = parse(outline, 0);
    const auto shifted = parse(shifted_outline, 4);
    if (!full || !shifted) {
        return 1;
    }

    std::cout << *full << " | " << *shifted << '\n';
    return 0;
}
