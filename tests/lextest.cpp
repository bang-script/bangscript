#include "lexer.h"
#include <iostream>

int main() {
    std::string source = R"(
let x = 42
const _pi = 3.14159
fn add(a, b) { return a + b }
let s = !String "hello"
let m = !~Integer "nope"
let q = ?Float 3.14
let d = !!List<Integer> [[1, 2], [3, 4]]
)";

    bang::Lexer lexer(source);
    auto tokens = lexer.scan();

    for (const auto& tok : tokens) {
        std::cout << static_cast<int>(tok.type) << " '" << tok.lexeme
                  << "' @ " << tok.line << ":" << tok.column;
        if (tok.error_msg) {
            std::cout << " ERROR: " << *tok.error_msg;
        }
        std::cout << "\n";
    }
}

