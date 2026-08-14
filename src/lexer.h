#pragma once

#include <string>
#include <vector>
#include <optional>

namespace bang {

enum class TokenType {
    // Literals
    Integer,
    Float,
    String,
    True,
    False,
    Nil,

    // Identifiers
    Identifier,

    // Keywords
    Const,
    Else,
    Fn,
    For,
    If,
    Import,
    In,
    Ld,
    Let,
    Macro,
    Match,
    Return,
    Spawn,
    Type,
    Unsafe,
    While,
    Break,
    Catch,
    Export,
    From,
    Quote,
    Try,
    Unquote,
    Yield,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Equal,
    EqualEqual,
    Bang,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Ampersand,
    Pipe,
    Caret,
    Tilde,
    AmpersandAmpersand,
    PipePipe,
    LessLess,
    GreaterGreater,
    Colon,
    ColonColon,
    ColonEqual,
    Arrow,
    FatArrow,
    Dot,
    DotDot,
    DotDotEqual,
    Comma,
    Semicolon,

    // Delimiters
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,

    // Special
    Hash,
    At,
    Dollar,
    BangType,
    BangTildeType,
    BangBangType,
    QueryType,
    Newline,
    Eof,
    Error,
    Extern,
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
    size_t column;
    std::optional<std::string> error_msg;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> scan();

private:
    std::string source_;
    size_t start_ = 0;
    size_t current_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;

    bool is_at_end() const;
    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);

    void skip_whitespace();
    void skip_comment();
    void skip_block_comment();

    void scan_token();
    void string_literal();
    void number_literal();
    void identifier_or_keyword();
    void rbt_type(TokenType base_type);

    void add_token(TokenType type);
    void add_error_token(const std::string& msg);

    bool is_alpha(char c) const;
    bool is_digit(char c) const;
    bool is_alpha_numeric(char c) const;
    bool is_type_char(char c) const;

    std::vector<Token> tokens_;
};

} // namespace bang

