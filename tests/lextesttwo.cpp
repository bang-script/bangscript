#include "lexer.h"
#include <iostream>

int main() {
    std::string source = R"(
let x = 42
const _pi = 3.14159
fn add(a :: Integer, b :: Integer) :: Integer { 
  return a + b 
}
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
