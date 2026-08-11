#include "lexer.h"
#include <cctype>

namespace bang {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::scan() {
    while (!is_at_end()) {
        scan_token();
    }

    tokens_.push_back({TokenType::Eof, "", line_, column_, std::nullopt});
    return tokens_;
}

void Lexer::scan_token() {
    skip_whitespace();

    if (is_at_end()) return;

    start_ = current_;

    char c = advance();

    switch (c) {
        case '+': add_token(TokenType::Plus); break;
        case '*': add_token(TokenType::Star); break;
        case '%': add_token(TokenType::Percent); break;
        case '(': add_token(TokenType::LParen); break;
        case ')': add_token(TokenType::RParen); break;
        case '{': add_token(TokenType::LBrace); break;
        case '}': add_token(TokenType::RBrace); break;
        case '[': add_token(TokenType::LBracket); break;
        case ']': add_token(TokenType::RBracket); break;
        case ',': add_token(TokenType::Comma); break;
        case ';': add_token(TokenType::Semicolon); break;
        case '^': add_token(TokenType::Caret); break;
        case '~': add_token(TokenType::Tilde); break;
        case '#': add_token(TokenType::Hash); break;
        case '@': add_token(TokenType::At); break;
        case '$': add_token(TokenType::Dollar); break;

        case '-':
            if (match('>')) add_token(TokenType::Arrow);
            else add_token(TokenType::Minus);
            break;

        case '/':
            if (match('/')) {
                skip_comment();
            } else if (match('*')) {
                skip_block_comment();
            } else {
                add_token(TokenType::Slash);
            }
            break;

        case '=':
            if (match('=')) add_token(TokenType::EqualEqual);
            else if (match('>')) add_token(TokenType::FatArrow);
            else add_token(TokenType::Equal);
            break;

        case '!':
            if (match('~')) {
                if (is_alpha(peek())) {
                    rbt_type(TokenType::BangTildeType);
                } else {
                    add_error_token("Expected type name after !~");
                }
            } else if (is_alpha(peek())) {
                rbt_type(TokenType::BangType);
            } else if (match('=')) {
                add_token(TokenType::BangEqual);
            } else {
                add_token(TokenType::Bang);
            }
            break;

        case '?':
            if (is_alpha(peek())) {
                rbt_type(TokenType::QueryType);
            } else {
                add_error_token("Expected type name after ?");
            }
            break;

        case ':':
            if (match(':')) add_token(TokenType::ColonColon);
            else if (match('=')) add_token(TokenType::ColonEqual);
            else add_token(TokenType::Colon);
            break;

        case '<':
            if (match('=')) add_token(TokenType::LessEqual);
            else if (match('<')) add_token(TokenType::LessLess);
            else add_token(TokenType::Less);
            break;

        case '>':
            if (match('=')) add_token(TokenType::GreaterEqual);
            else if (match('>')) add_token(TokenType::GreaterGreater);
            else add_token(TokenType::Greater);
            break;

        case '&':
            if (match('&')) add_token(TokenType::AmpersandAmpersand);
            else add_token(TokenType::Ampersand);
            break;

        case '|':
            if (match('|')) add_token(TokenType::PipePipe);
            else add_token(TokenType::Pipe);
            break;

        case '.':
            if (match('.')) {
                if (match('=')) add_token(TokenType::DotDotEqual);
                else add_token(TokenType::DotDot);
            } else {
                add_token(TokenType::Dot);
            }
            break;

        case '"': string_literal(); break;

        case '\n':
            add_token(TokenType::Newline);
            line_++;
            column_ = 1;
            break;

        default:
            if (is_digit(c)) {
                number_literal();
            } else if (is_alpha(c)) {
                identifier_or_keyword();
            } else {
                add_error_token("Unexpected character: '" + std::string(1, c) + "'");
            }
            break;
    }
}

void Lexer::string_literal() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            add_error_token("Unterminated string (newline in string)");
            return;
        }
        if (peek() == '\\') {
            advance();
            if (is_at_end()) {
                add_error_token("Unterminated string escape");
                return;
            }
            char esc = peek();
            if (esc != 'n' && esc != 't' && esc != '\\' && esc != '"' && esc != 'r' && esc != '0') {
                add_error_token("Unknown escape sequence: \\" + std::string(1, esc));
                return;
            }
            advance();
        } else {
            advance();
        }
    }

    if (is_at_end()) {
        add_error_token("Unterminated string");
        return;
    }

    advance();
    add_token(TokenType::String);
}

void Lexer::number_literal() {
    while (is_digit(peek())) advance();

    bool is_float = false;

    if (peek() == '.' && is_digit(peek_next())) {
        is_float = true;
        advance();
        while (is_digit(peek())) advance();
    }

    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();

        if (peek() == '+' || peek() == '-') {
            advance();
        }

        if (!is_digit(peek())) {
            add_error_token("Invalid exponent in number literal");
            return;
        }

        while (is_digit(peek())) advance();
    }

    add_token(is_float ? TokenType::Float : TokenType::Integer);
}

void Lexer::identifier_or_keyword() {
    while (is_alpha_numeric(peek())) advance();

    std::string text = source_.substr(start_, current_ - start_);

    TokenType type = TokenType::Identifier;

    if (text == "const") type = TokenType::Const;
    else if (text == "else") type = TokenType::Else;
    else if (text == "fn") type = TokenType::Fn;
    else if (text == "for") type = TokenType::For;
    else if (text == "if") type = TokenType::If;
    else if (text == "import") type = TokenType::Import;
    else if (text == "in") type = TokenType::In;
    else if (text == "ld") type = TokenType::Ld;
    else if (text == "let") type = TokenType::Let;
    else if (text == "macro") type = TokenType::Macro;
    else if (text == "match") type = TokenType::Match;
    else if (text == "nil") type = TokenType::Nil;
    else if (text == "return") type = TokenType::Return;
    else if (text == "spawn") type = TokenType::Spawn;
    else if (text == "type") type = TokenType::Type;
    else if (text == "unsafe") type = TokenType::Unsafe;
    else if (text == "while") type = TokenType::While;
    else if (text == "break") type = TokenType::Break;
    else if (text == "catch") type = TokenType::Catch;
    else if (text == "export") type = TokenType::Export;
    else if (text == "from") type = TokenType::From;
    else if (text == "quote") type = TokenType::Quote;
    else if (text == "try") type = TokenType::Try;
    else if (text == "unquote") type = TokenType::Unquote;
    else if (text == "yield") type = TokenType::Yield;
    else if (text == "true") type = TokenType::True;
    else if (text == "false") type = TokenType::False;

    add_token(type);
}

void Lexer::rbt_type(TokenType base_type) {
    while (is_type_char(peek())) advance();
    add_token(base_type);
}

bool Lexer::is_at_end() const {
    return current_ >= source_.size();
}

char Lexer::advance() {
    column_++;
    return source_[current_++];
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.size()) return '\0';
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (source_[current_] != expected) return false;
    current_++;
    column_++;
    return true;
}

void Lexer::skip_whitespace() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skip_comment() {
    while (peek() != '\n' && !is_at_end()) advance();
}

void Lexer::skip_block_comment() {
    int depth = 1;
    while (!is_at_end() && depth > 0) {
        if (peek() == '/' && peek_next() == '*') {
            advance();
            advance();
            depth++;
        } else if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            depth--;
        } else {
            if (peek() == '\n') {
                line_++;
                column_ = 1;
            }
            advance();
        }
    }
    if (depth > 0) {
        add_error_token("Unterminated block comment");
    }
}

void Lexer::add_token(TokenType type) {
    tokens_.push_back({
        type,
        source_.substr(start_, current_ - start_),
        line_,
        column_ - (current_ - start_),
        std::nullopt
    });
}

void Lexer::add_error_token(const std::string& msg) {
    tokens_.push_back({
        TokenType::Error,
        source_.substr(start_, current_ - start_),
        line_,
        column_ - (current_ - start_),
        msg
    });
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alpha_numeric(char c) const {
    return is_alpha(c) || is_digit(c);
}

bool Lexer::is_type_char(char c) const {
    return is_alpha_numeric(c) || c == '<' || c == '>' || c == '|' || c == ',' || c == '_';
}

} // namespace bang

