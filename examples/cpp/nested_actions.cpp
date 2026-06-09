#include <iostream>
#include <string>

std::string makeA() {
    return "A";
}

std::string makeB() {
    return "B";
}

// mparse: begin grammar

S<std::string>
    : A B C {
        $$ = $1 + $2 + $3
    }

A<std::string>
    : {
        $$ = makeA()
    }

B<std::string>
    : {
        $$ = makeB()
    }

C<std::string>
    : 'c' {
        $$ = $1
    }

// mparse: end grammar

int main() {
    const auto result = parse("c");
    if (!result) {
        return 1;
    }

    std::cout << *result << '\n';
    return 0;
}
