/*
Copyright (c) 2026 BangScript and Contributors
--- License: MIT ---
*/
// ============================================================================
// BangScript Lexer
// Tokenizes .bs source into a stream of rich Tokens with source locations.
// ============================================================================

use std::fmt;

// ============================================================================
// Token Kinds
// ============================================================================

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum TokenKind {
    // Literals
    Integer(i64),
    Float(f64),
    String(String),      // "hello" or "{x}"
    RawString(String),   // """raw"""
    Bool(bool),
    Nil,

    // Identifiers
    Ident(String),

    // Keywords
    Let,
    Const,
    Mut,
    Fn,
    Ld,
    If,
    Else,
    Match,
    For,
    In,
    While,
    Break,
    Continue,
    Return,
    Type,
    Import,
    Export,
    From,
    As,
    Unsafe,
    Spawn,
    Try,
    Catch,
    Quote,
    Unquote,
    Macro,
    True,
    False,
    NilKw,

    // Operators
    Bang,           // !
    BangTilde,      // !~
    Question,       // ?
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    Eq,             // ==
    Neq,            // !=
    Lt,             // <
    Gt,             // >
    Leq,            // <=
    Geq,            // >=
    And,            // &&
    Or,             // ||
    Not,            // ! (logical not, distinguished from Bang by context)
    Assign,         // =
    Arrow,          // ->
    FatArrow,       // =>
    DoubleColon,    // ::
    Colon,          // :
    Dot,            // .
    DotDot,         // ..
    DotDotEq,       // ..=
    Comma,          // ,
    Semi,           // ;
    Pipe,           // |

    // Delimiters
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    LBracket,       // [
    RBracket,       // ]

    // Special
    Newline,
    Eof,
    Comment,        // Consumed by lexer, not emitted
    DocComment(String),
}

impl fmt::Display for TokenKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TokenKind::Integer(n) => write!(f, "integer '{}'", n),
            TokenKind::Float(n) => write!(f, "float '{}'", n),
            TokenKind::String(s) => write!(f, "string \"{}\"", s),
            TokenKind::RawString(s) => write!(f, "raw string"),
            TokenKind::Bool(b) => write!(f, "bool '{}'", b),
            TokenKind::Nil => write!(f, "nil"),
            TokenKind::Ident(s) => write!(f, "identifier '{}'", s),
            TokenKind::Let => write!(f, "'let'"),
            TokenKind::Const => write!(f, "'const'"),
            TokenKind::Mut => write!(f, "'mut'"),
            TokenKind::Fn => write!(f, "'fn'"),
            TokenKind::Ld => write!(f, "'ld'"),
            TokenKind::If => write!(f, "'if'"),
            TokenKind::Else => write!(f, "'else'"),
            TokenKind::Match => write!(f, "'match'"),
            TokenKind::For => write!(f, "'for'"),
            TokenKind::In => write!(f, "'in'"),
            TokenKind::While => write!(f, "'while'"),
            TokenKind::Break => write!(f, "'break'"),
            TokenKind::Continue => write!(f, "'continue'"),
            TokenKind::Return => write!(f, "'return'"),
            TokenKind::Type => write!(f, "'type'"),
            TokenKind::Import => write!(f, "'import'"),
            TokenKind::Export => write!(f, "'export'"),
            TokenKind::From => write!(f, "'from'"),
            TokenKind::As => write!(f, "'as'"),
            TokenKind::Unsafe => write!(f, "'unsafe'"),
            TokenKind::Spawn => write!(f, "'spawn'"),
            TokenKind::Try => write!(f, "'try'"),
            TokenKind::Catch => write!(f, "'catch'"),
            TokenKind::Quote => write!(f, "'quote'"),
            TokenKind::Unquote => write!(f, "'unquote'"),
            TokenKind::Macro => write!(f, "'macro'"),
            TokenKind::True => write!(f, "'true'"),
            TokenKind::False => write!(f, "'false'"),
            TokenKind::NilKw => write!(f, "'nil'"),
            TokenKind::Bang => write!(f, "'!'"),
            TokenKind::BangTilde => write!(f, "'!~'"),
            TokenKind::Question => write!(f, "'?'"),
            TokenKind::Plus => write!(f, "'+'"),
            TokenKind::Minus => write!(f, "'-'"),
            TokenKind::Star => write!(f, "'*'"),
            TokenKind::Slash => write!(f, "'/'"),
            TokenKind::Percent => write!(f, "'%'"),
            TokenKind::Eq => write!(f, "'=='"),
            TokenKind::Neq => write!(f, "'!='"),
            TokenKind::Lt => write!(f, "'<'"),
            TokenKind::Gt => write!(f, "'>'"),
            TokenKind::Leq => write!(f, "'<='"),
            TokenKind::Geq => write!(f, "'>='"),
            TokenKind::And => write!(f, "'&&'"),
            TokenKind::Or => write!(f, "'||'"),
            TokenKind::Not => write!(f, "'!'"),
            TokenKind::Assign => write!(f, "'='"),
            TokenKind::Arrow => write!(f, "'->'"),
            TokenKind::FatArrow => write!(f, "'=>'"),
            TokenKind::DoubleColon => write!(f, "'::'"),
            TokenKind::Colon => write!(f, "':'"),
            TokenKind::Dot => write!(f, "'.'"),
            TokenKind::DotDot => write!(f, "'..'"),
            TokenKind::DotDotEq => write!(f, "'..='"),
            TokenKind::Comma => write!(f, "','"),
            TokenKind::Semi => write!(f, "';'"),
            TokenKind::Pipe => write!(f, "'|'"),
            TokenKind::LParen => write!(f, "'('"),
            TokenKind::RParen => write!(f, "')'"),
            TokenKind::LBrace => write!(f, "'{'"),
            TokenKind::RBrace => write!(f, "'}'"),
            TokenKind::LBracket => write!(f, "'['"),
            TokenKind::RBracket => write!(f, "']'"),
            TokenKind::Newline => write!(f, "newline"),
            TokenKind::Eof => write!(f, "end of file"),
            TokenKind::Comment => write!(f, "comment"),
            TokenKind::DocComment(s) => write!(f, "doc comment"),
        }
    }
}

// ============================================================================
// Source Location
// ============================================================================

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Span {
    pub start: usize,   // Byte offset in source
    pub end: usize,     // Byte offset in source
}

impl Span {
    pub fn new(start: usize, end: usize) -> Self {
        Self { start, end }
    }

    pub fn merge(a: Span, b: Span) -> Span {
        Span {
            start: a.start.min(b.start),
            end: a.end.max(b.end),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Loc {
    pub line: usize,
    pub col: usize,
    pub span: Span,
}

impl Loc {
    pub fn new(line: usize, col: usize, start: usize, end: usize) -> Self {
        Self {
            line,
            col,
            span: Span::new(start, end),
        }
    }
}

// ============================================================================
// Token
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub struct Token {
    pub kind: TokenKind,
    pub loc: Loc,
}

impl Token {
    pub fn new(kind: TokenKind, line: usize, col: usize, start: usize, end: usize) -> Self {
        Self {
            kind,
            loc: Loc::new(line, col, start, end),
        }
    }
}

// ============================================================================
// Lexer Error
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub struct LexError {
    pub message: String,
    pub loc: Loc,
}

impl fmt::Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Lex error at {}:{}: {}", self.loc.line, self.loc.col, self.message)
    }
}

impl std::error::Error for LexError {}

// ============================================================================
// Lexer
// ============================================================================

pub struct Lexer<'a> {
    source: &'a str,
    bytes: &'a [u8],
    pos: usize,         // Current byte position
    line: usize,        // Current line (1-indexed)
    col: usize,         // Current column (1-indexed)
    line_start: usize,  // Byte position where current line starts
}

impl<'a> Lexer<'a> {
    pub fn new(source: &'a str) -> Self {
        Self {
            source,
            bytes: source.as_bytes(),
            pos: 0,
            line: 1,
            col: 1,
            line_start: 0,
        }
    }

    // ------------------------------------------------------------------------
    // Character access
    // ------------------------------------------------------------------------

    fn peek(&self) -> char {
        self.peek_at(0)
    }

    fn peek_at(&self, offset: usize) -> char {
        match self.bytes.get(self.pos + offset) {
            Some(&b) => b as char,
            None => '\0',
        }
    }

    fn is_at_end(&self) -> bool {
        self.pos >= self.bytes.len()
    }

    fn advance(&mut self) -> char {
        let ch = self.peek();
        self.pos += 1;
        if ch == '\n' {
            self.line += 1;
            self.col = 1;
            self.line_start = self.pos;
        } else if ch == '\t' {
            self.col += 4; // Tab = 4 spaces
        } else {
            self.col += 1;
        }
        ch
    }

    fn match_char(&mut self, expected: char) -> bool {
        if self.peek() == expected {
            self.advance();
            true
        } else {
            false
        }
    }

    fn skip_whitespace(&mut self) {
        while !self.is_at_end() {
            match self.peek() {
                ' ' | '\r' | '\t' => { self.advance(); }
                _ => break,
            }
        }
    }

    // ------------------------------------------------------------------------
    // Location helpers
    // ------------------------------------------------------------------------

    fn current_loc(&self, start: usize) -> Loc {
        Loc::new(self.line, self.col, start, self.pos)
    }

    // ------------------------------------------------------------------------
    // Comments
    // ------------------------------------------------------------------------

    fn read_line_comment(&mut self) -> Option<Token> {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        self.advance(); // /
        self.advance(); // /

        // Check for doc comment (/// but not ////)
        let is_doc = self.peek() == '/' && self.peek_at(1) != '/';

        let mut content = String::new();
        while !self.is_at_end() && self.peek() != '\n' {
            content.push(self.advance());
        }

        if is_doc {
            // Strip the leading '/'
            let doc_content = content.trim_start_matches('/').trim().to_string();
            Some(Token::new(
                TokenKind::DocComment(doc_content),
                start_line,
                start_col,
                start,
                self.pos,
            ))
        } else {
            None // Regular comments are not emitted as tokens
        }
    }

    fn read_block_comment(&mut self) -> Result<Option<Token>, LexError> {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        self.advance(); // /
        self.advance(); // *

        // Check for doc comment (/** ... */)
        let is_doc = self.peek() == '*' && self.peek_at(1) != '/' && self.peek_at(1) != '*';

        let mut depth = 1;
        let mut content = String::new();

        while !self.is_at_end() && depth > 0 {
            if self.peek() == '/' && self.peek_at(1) == '*' {
                depth += 1;
                self.advance();
                self.advance();
            } else if self.peek() == '*' && self.peek_at(1) == '/' {
                depth -= 1;
                if depth > 0 {
                    content.push(self.advance());
                    content.push(self.advance());
                } else {
                    self.advance(); // *
                    self.advance(); // /
                }
            } else {
                content.push(self.advance());
            }
        }

        if depth > 0 {
            return Err(LexError {
                message: "Unterminated block comment".to_string(),
                loc: Loc::new(start_line, start_col, start, self.pos),
            });
        }

        if is_doc {
            Ok(Some(Token::new(
                TokenKind::DocComment(content.trim().to_string()),
                start_line,
                start_col,
                start,
                self.pos,
            )))
        } else {
            Ok(None) // Regular block comments are not emitted
        }
    }

    // ------------------------------------------------------------------------
    // String literals
    // ------------------------------------------------------------------------

    fn read_string(&mut self) -> Result<Token, LexError> {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        let quote = self.advance(); // " or '
        let mut value = String::new();

        while !self.is_at_end() && self.peek() != quote {
            if self.peek() == '\\' {
                self.advance();
                match self.peek() {
                    'n' => { value.push('\n'); self.advance(); }
                    't' => { value.push('\t'); self.advance(); }
                    'r' => { value.push('\r'); self.advance(); }
                    '\\' => { value.push('\\'); self.advance(); }
                    '"' => { value.push('"'); self.advance(); }
                    '\'' => { value.push('\''); self.advance(); }
                    '0' => { value.push('\0'); self.advance(); }
                    'x' => {
                        self.advance();
                        let hex = self.read_hex_escape(2)?;
                        value.push(hex as char);
                    }
                    'u' => {
                        self.advance();
                        if self.peek() == '{' {
                            self.advance();
                            let hex_str = self.read_hex_digits();
                            if self.peek() != '}' {
                                return Err(LexError {
                                    message: "Expected '}' to close \\u{...}".to_string(),
                                    loc: self.current_loc(start),
                                });
                            }
                            self.advance();
                            match u32::from_str_radix(&hex_str, 16) {
                                Ok(codepoint) => match char::from_u32(codepoint) {
                                    Some(ch) => value.push(ch),
                                    None => return Err(LexError {
                                        message: format!("Invalid unicode codepoint: {}", codepoint),
                                        loc: self.current_loc(start),
                                    }),
                                },
                                Err(_) => return Err(LexError {
                                    message: format!("Invalid hex escape: {}", hex_str),
                                    loc: self.current_loc(start),
                                }),
                            }
                        } else {
                            let hex = self.read_hex_escape(4)?;
                            match char::from_u32(hex) {
                                Some(ch) => value.push(ch),
                                None => return Err(LexError {
                                    message: format!("Invalid unicode escape: {:04x}", hex),
                                    loc: self.current_loc(start),
                                }),
                            }
                        }
                    }
                    ch => {
                        return Err(LexError {
                            message: format!("Unknown escape sequence: \\{}", ch),
                            loc: self.current_loc(start),
                        });
                    }
                }
            } else if self.peek() == '\n' {
                return Err(LexError {
                    message: "Unterminated string literal".to_string(),
                    loc: self.current_loc(start),
                });
            } else {
                value.push(self.advance());
            }
        }

        if self.is_at_end() {
            return Err(LexError {
                message: "Unterminated string literal".to_string(),
                loc: self.current_loc(start),
            });
        }

        self.advance(); // closing quote

        Ok(Token::new(
            TokenKind::String(value),
            start_line,
            start_col,
            start,
            self.pos,
        ))
    }

    fn read_raw_string(&mut self) -> Result<Token, LexError> {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        self.advance(); // "
        self.advance(); // "
        self.advance(); // "

        let mut value = String::new();

        while !self.is_at_end() {
            if self.peek() == '"' && self.peek_at(1) == '"' && self.peek_at(2) == '"' {
                self.advance();
                self.advance();
                self.advance();
                break;
            }
            value.push(self.advance());
        }

        if self.is_at_end() {
            return Err(LexError {
                message: "Unterminated raw string literal".to_string(),
                loc: self.current_loc(start),
            });
        }

        Ok(Token::new(
            TokenKind::RawString(value),
            start_line,
            start_col,
            start,
            self.pos,
        ))
    }

    fn read_hex_escape(&mut self, count: usize) -> Result<u32, LexError> {
        let start = self.pos;
        let mut hex_str = String::new();
        for _ in 0..count {
            let ch = self.peek();
            if ch.is_ascii_hexdigit() {
                hex_str.push(self.advance());
            } else {
                return Err(LexError {
                    message: format!("Expected {} hex digits, got '{}'", count, hex_str.len()),
                    loc: self.current_loc(start),
                });
            }
        }
        match u32::from_str_radix(&hex_str, 16) {
            Ok(n) => Ok(n),
            Err(_) => Err(LexError {
                message: format!("Invalid hex escape: {}", hex_str),
                loc: self.current_loc(start),
            }),
        }
    }

    fn read_hex_digits(&mut self) -> String {
        let mut hex_str = String::new();
        while !self.is_at_end() && self.peek().is_ascii_hexdigit() {
            hex_str.push(self.advance());
        }
        hex_str
    }

    // ------------------------------------------------------------------------
    // Number literals
    // ------------------------------------------------------------------------

    fn read_number(&mut self) -> Result<Token, LexError> {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        // Check for hex/octal/binary
        if self.peek() == '0' {
            let next = self.peek_at(1);
            if next == 'x' || next == 'X' {
                return self.read_hex_number(start, start_line, start_col);
            } else if next == 'o' || next == 'O' {
                return self.read_octal_number(start, start_line, start_col);
            } else if next == 'b' || next == 'B' {
                return self.read_binary_number(start, start_line, start_col);
            }
        }

        let mut num_str = String::new();

        while !self.is_at_end() && self.peek().is_ascii_digit() {
            num_str.push(self.advance());
        }

        // Float with decimal point
        if self.peek() == '.' && self.peek_at(1).is_ascii_digit() {
            num_str.push(self.advance()); // .
            while !self.is_at_end() && self.peek().is_ascii_digit() {
                num_str.push(self.advance());
            }

            // Scientific notation
            if self.peek() == 'e' || self.peek() == 'E' {
                num_str.push(self.advance());
                if self.peek() == '+' || self.peek() == '-' {
                    num_str.push(self.advance());
                }
                if !self.peek().is_ascii_digit() {
                    return Err(LexError {
                        message: "Expected digits in exponent".to_string(),
                        loc: self.current_loc(start),
                    });
                }
                while !self.is_at_end() && self.peek().is_ascii_digit() {
                    num_str.push(self.advance());
                }
            }

            match num_str.parse::<f64>() {
                Ok(n) => Ok(Token::new(
                    TokenKind::Float(n),
                    start_line,
                    start_col,
                    start,
                    self.pos,
                )),
                Err(_) => Err(LexError {
                    message: format!("Invalid float literal: {}", num_str),
                    loc: self.current_loc(start),
                }),
            }
        } else {
            // Integer
            match num_str.parse::<i64>() {
                Ok(n) => Ok(Token::new(
                    TokenKind::Integer(n),
                    start_line,
                    start_col,
                    start,
                    self.pos,
                )),
                Err(_) => Err(LexError {
                    message: format!("Invalid integer literal: {}", num_str),
                    loc: self.current_loc(start),
                }),
            }
        }
    }

    fn read_hex_number(&mut self, start: usize, start_line: usize, start_col: usize) -> Result<Token, LexError> {
        self.advance(); // 0
        self.advance(); // x

        let mut hex_str = String::new();
        while !self.is_at_end() && self.peek().is_ascii_hexdigit() {
            hex_str.push(self.advance());
        }

        if hex_str.is_empty() {
            return Err(LexError {
                message: "Expected hex digits after 0x".to_string(),
                loc: self.current_loc(start),
            });
        }

        match i64::from_str_radix(&hex_str, 16) {
            Ok(n) => Ok(Token::new(
                TokenKind::Integer(n),
                start_line,
                start_col,
                start,
                self.pos,
            )),
            Err(_) => Err(LexError {
                message: format!("Invalid hex literal: 0x{}", hex_str),
                loc: self.current_loc(start),
            }),
        }
    }

    fn read_octal_number(&mut self, start: usize, start_line: usize, start_col: usize) -> Result<Token, LexError> {
        self.advance(); // 0
        self.advance(); // o

        let mut oct_str = String::new();
        while !self.is_at_end() && self.peek().is_ascii_digit() && self.peek() < '8' {
            oct_str.push(self.advance());
        }

        if oct_str.is_empty() {
            return Err(LexError {
                message: "Expected octal digits after 0o".to_string(),
                loc: self.current_loc(start),
            });
        }

        match i64::from_str_radix(&oct_str, 8) {
            Ok(n) => Ok(Token::new(
                TokenKind::Integer(n),
                start_line,
                start_col,
                start,
                self.pos,
            )),
            Err(_) => Err(LexError {
                message: format!("Invalid octal literal: 0o{}", oct_str),
                loc: self.current_loc(start),
            }),
        }
    }

    fn read_binary_number(&mut self, start: usize, start_line: usize, start_col: usize) -> Result<Token, LexError> {
        self.advance(); // 0
        self.advance(); // b

        let mut bin_str = String::new();
        while !self.is_at_end() && (self.peek() == '0' || self.peek() == '1') {
            bin_str.push(self.advance());
        }

        if bin_str.is_empty() {
            return Err(LexError {
                message: "Expected binary digits after 0b".to_string(),
                loc: self.current_loc(start),
            });
        }

        match i64::from_str_radix(&bin_str, 2) {
            Ok(n) => Ok(Token::new(
                TokenKind::Integer(n),
                start_line,
                start_col,
                start,
                self.pos,
            )),
            Err(_) => Err(LexError {
                message: format!("Invalid binary literal: 0b{}", bin_str),
                loc: self.current_loc(start),
            }),
        }
    }

    // ------------------------------------------------------------------------
    // Identifiers and keywords
    // ------------------------------------------------------------------------

    fn read_identifier(&mut self) -> Token {
        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;

        let mut ident = String::new();

        while !self.is_at_end() && (self.peek().is_alphanumeric() || self.peek() == '_') {
            ident.push(self.advance());
        }

        let kind = match ident.as_str() {
            "let" => TokenKind::Let,
            "const" => TokenKind::Const,
            "mut" => TokenKind::Mut,
            "fn" => TokenKind::Fn,
            "ld" => TokenKind::Ld,
            "if" => TokenKind::If,
            "else" => TokenKind::Else,
            "match" => TokenKind::Match,
            "for" => TokenKind::For,
            "in" => TokenKind::In,
            "while" => TokenKind::While,
            "break" => TokenKind::Break,
            "continue" => TokenKind::Continue,
            "return" => TokenKind::Return,
            "type" => TokenKind::Type,
            "import" => TokenKind::Import,
            "export" => TokenKind::Export,
            "from" => TokenKind::From,
            "as" => TokenKind::As,
            "unsafe" => TokenKind::Unsafe,
            "spawn" => TokenKind::Spawn,
            "try" => TokenKind::Try,
            "catch" => TokenKind::Catch,
            "quote" => TokenKind::Quote,
            "unquote" => TokenKind::Unquote,
            "macro" => TokenKind::Macro,
            "true" => TokenKind::Bool(true),
            "false" => TokenKind::Bool(false),
            "nil" => TokenKind::NilKw,
            _ => TokenKind::Ident(ident),
        };

        Token::new(kind, start_line, start_col, start, self.pos)
    }

    // ------------------------------------------------------------------------
    // Single token
    // ------------------------------------------------------------------------

    pub fn next_token(&mut self) -> Result<Token, LexError> {
        self.skip_whitespace();

        if self.is_at_end() {
            let line = self.line;
            let col = self.col;
            return Ok(Token::new(TokenKind::Eof, line, col, self.pos, self.pos));
        }

        let start = self.pos;
        let start_line = self.line;
        let start_col = self.col;
        let ch = self.peek();

        // Newline (significant in BangScript)
        if ch == '\n' {
            self.advance();
            return Ok(Token::new(TokenKind::Newline, start_line, start_col, start, self.pos));
        }

        // Comments
        if ch == '/' {
            if self.peek_at(1) == '/' {
                return match self.read_line_comment() {
                    Some(tok) => Ok(tok),
                    None => self.next_token(), // Skip regular comment, get next token
                };
            }
            if self.peek_at(1) == '*' {
                return match self.read_block_comment()? {
                    Some(tok) => Ok(tok),
                    None => self.next_token(), // Skip regular comment, get next token
                };
            }
        }

        // String literals
        if ch == '"' {
            if self.peek_at(1) == '"' && self.peek_at(2) == '"' {
                return self.read_raw_string();
            }
            return self.read_string();
        }

        // Numbers
        if ch.is_ascii_digit() {
            return self.read_number();
        }

        // Identifiers and keywords
        if ch.is_alphabetic() || ch == '_' {
            return Ok(self.read_identifier());
        }

        // Operators and delimiters
        self.advance();

        let kind = match ch {
            '+' => TokenKind::Plus,
            '-' => {
                if self.match_char('>') {
                    TokenKind::Arrow
                } else {
                    TokenKind::Minus
                }
            }
            '*' => TokenKind::Star,
            '/' => TokenKind::Slash,
            '%' => TokenKind::Percent,
            '!' => {
                if self.match_char('=') {
                    TokenKind::Neq
                } else if self.match_char('~') {
                    TokenKind::BangTilde
                } else {
                    TokenKind::Bang
                }
            }
            '?' => TokenKind::Question,
            '=' => {
                if self.match_char('=') {
                    TokenKind::Eq
                } else if self.match_char('>') {
                    TokenKind::FatArrow
                } else {
                    TokenKind::Assign
                }
            }
            '<' => {
                if self.match_char('=') {
                    TokenKind::Leq
                } else {
                    TokenKind::Lt
                }
            }
            '>' => {
                if self.match_char('=') {
                    TokenKind::Geq
                } else {
                    TokenKind::Gt
                }
            }
            '&' => {
                if self.match_char('&') {
                    TokenKind::And
                } else {
                    return Err(LexError {
                        message: "Expected '&&', got '&'".to_string(),
                        loc: self.current_loc(start),
                    });
                }
            }
            '|' => {
                if self.match_char('|') {
                    TokenKind::Or
                } else {
                    TokenKind::Pipe
                }
            }
            ':' => {
                if self.match_char(':') {
                    TokenKind::DoubleColon
                } else {
                    TokenKind::Colon
                }
            }
            '.' => {
                if self.match_char('.') {
                    if self.match_char('=') {
                        TokenKind::DotDotEq
                    } else {
                        TokenKind::DotDot
                    }
                } else {
                    TokenKind::Dot
                }
            }
            ',' => TokenKind::Comma,
            ';' => TokenKind::Semi,
            '(' => TokenKind::LParen,
            ')' => TokenKind::RParen,
            '{' => TokenKind::LBrace,
            '}' => TokenKind::RBrace,
            '[' => TokenKind::LBracket,
            ']' => TokenKind::RBracket,
            _ => {
                return Err(LexError {
                    message: format!("Unexpected character '{}'", ch),
                    loc: self.current_loc(start),
                });
            }
        };

        Ok(Token::new(kind, start_line, start_col, start, self.pos))
    }

    // ------------------------------------------------------------------------
    // Tokenize entire source
    // ------------------------------------------------------------------------

    pub fn tokenize(mut self) -> Result<Vec<Token>, LexError> {
        let mut tokens = Vec::new();

        loop {
            let token = self.next_token()?;
            let is_eof = token.kind == TokenKind::Eof;
            tokens.push(token);
            if is_eof {
                break;
            }
        }

        Ok(tokens)
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn lex(source: &str) -> Vec<TokenKind> {
        let lexer = Lexer::new(source);
        lexer.tokenize()
            .unwrap()
            .into_iter()
            .map(|t| t.kind)
            .collect()
    }

    fn lex_err(source: &str) -> String {
        let lexer = Lexer::new(source);
        lexer.tokenize().unwrap_err().message
    }

    #[test]
    fn test_empty() {
        let tokens = lex("");
        assert_eq!(tokens, vec![TokenKind::Eof]);
    }

    #[test]
    fn test_integers() {
        let tokens = lex("42 0 1234567890");
        assert!(matches!(tokens[0], TokenKind::Integer(42)));
        assert!(matches!(tokens[1], TokenKind::Integer(0)));
        assert!(matches!(tokens[2], TokenKind::Integer(1234567890)));
    }

    #[test]
    fn test_floats() {
        let tokens = lex("3.14 0.5 1e10 2.5e-3");
        assert!(matches!(tokens[0], TokenKind::Float(f) if (f - 3.14).abs() < 0.001));
        assert!(matches!(tokens[1], TokenKind::Float(f) if (f - 0.5).abs() < 0.001));
    }

    #[test]
    fn test_hex() {
        let tokens = lex("0xFF 0x0 0xDEADBEEF");
        assert!(matches!(tokens[0], TokenKind::Integer(255)));
        assert!(matches!(tokens[1], TokenKind::Integer(0)));
        assert!(matches!(tokens[2], TokenKind::Integer(3735928559)));
    }

    #[test]
    fn test_binary() {
        let tokens = lex("0b1010 0b0 0b11111111");
        assert!(matches!(tokens[0], TokenKind::Integer(10)));
        assert!(matches!(tokens[1], TokenKind::Integer(0)));
        assert!(matches!(tokens[2], TokenKind::Integer(255)));
    }

    #[test]
    fn test_strings() {
        let tokens = lex(r#""hello" "world" "with\nescapes""#);
        assert!(matches!(&tokens[0], TokenKind::String(s) if s == "hello"));
        assert!(matches!(&tokens[1], TokenKind::String(s) if s == "world"));
        assert!(matches!(&tokens[2], TokenKind::String(s) if s == "with\nescapes"));
    }

    #[test]
    fn test_raw_string() {
        let tokens = lex(r#"""raw
string""""#);
        assert!(matches!(&tokens[0], TokenKind::RawString(s) if s == "raw\nstring"));
    }

    #[test]
    fn test_keywords() {
        let tokens = lex("let const fn ld if else match for in while return type import export");
        assert_eq!(tokens[0], TokenKind::Let);
        assert_eq!(tokens[1], TokenKind::Const);
        assert_eq!(tokens[2], TokenKind::Fn);
        assert_eq!(tokens[3], TokenKind::Ld);
        assert_eq!(tokens[4], TokenKind::If);
        assert_eq!(tokens[5], TokenKind::Else);
        assert_eq!(tokens[6], TokenKind::Match);
        assert_eq!(tokens[7], TokenKind::For);
        assert_eq!(tokens[8], TokenKind::In);
        assert_eq!(tokens[9], TokenKind::While);
        assert_eq!(tokens[10], TokenKind::Return);
        assert_eq!(tokens[11], TokenKind::Type);
        assert_eq!(tokens[12], TokenKind::Import);
        assert_eq!(tokens[13], TokenKind::Export);
    }

    #[test]
    fn test_booleans_and_nil() {
        let tokens = lex("true false nil");
        assert_eq!(tokens[0], TokenKind::Bool(true));
        assert_eq!(tokens[1], TokenKind::Bool(false));
        assert_eq!(tokens[2], TokenKind::NilKw);
    }

    #[test]
    fn test_operators() {
        let tokens = lex("+ - * / % == != < > <= >= && || = -> => :: : . .. ..= , ; |");
        assert_eq!(tokens[0], TokenKind::Plus);
        assert_eq!(tokens[1], TokenKind::Minus);
        assert_eq!(tokens[2], TokenKind::Star);
        assert_eq!(tokens[3], TokenKind::Slash);
        assert_eq!(tokens[4], TokenKind::Percent);
        assert_eq!(tokens[5], TokenKind::Eq);
        assert_eq!(tokens[6], TokenKind::Neq);
        assert_eq!(tokens[7], TokenKind::Lt);
        assert_eq!(tokens[8], TokenKind::Gt);
        assert_eq!(tokens[9], TokenKind::Leq);
        assert_eq!(tokens[10], TokenKind::Geq);
        assert_eq!(tokens[11], TokenKind::And);
        assert_eq!(tokens[12], TokenKind::Or);
        assert_eq!(tokens[13], TokenKind::Assign);
        assert_eq!(tokens[14], TokenKind::Arrow);
        assert_eq!(tokens[15], TokenKind::FatArrow);
        assert_eq!(tokens[16], TokenKind::DoubleColon);
        assert_eq!(tokens[17], TokenKind::Colon);
        assert_eq!(tokens[18], TokenKind::Dot);
        assert_eq!(tokens[19], TokenKind::DotDot);
        assert_eq!(tokens[20], TokenKind::DotDotEq);
        assert_eq!(tokens[21], TokenKind::Comma);
        assert_eq!(tokens[22], TokenKind::Semi);
        assert_eq!(tokens[23], TokenKind::Pipe);
    }

    #[test]
    fn test_rbt_operators() {
        let tokens = lex("!Integer !~Integer ?Integer");
        assert_eq!(tokens[0], TokenKind::Bang);
        assert!(matches!(&tokens[1], TokenKind::Ident(s) if s == "Integer"));
        assert_eq!(tokens[2], TokenKind::BangTilde);
        assert!(matches!(&tokens[3], TokenKind::Ident(s) if s == "Integer"));
        assert_eq!(tokens[4], TokenKind::Question);
        assert!(matches!(&tokens[5], TokenKind::Ident(s) if s == "Integer"));
    }

    #[test]
    fn test_delimiters() {
        let tokens = lex("() {} []");
        assert_eq!(tokens[0], TokenKind::LParen);
        assert_eq!(tokens[1], TokenKind::RParen);
        assert_eq!(tokens[2], TokenKind::LBrace);
        assert_eq!(tokens[3], TokenKind::RBrace);
        assert_eq!(tokens[4], TokenKind::LBracket);
        assert_eq!(tokens[5], TokenKind::RBracket);
    }

    #[test]
    fn test_newlines() {
        let tokens = lex("let x = 1\nlet y = 2");
        assert_eq!(tokens[0], TokenKind::Let);
        assert!(matches!(&tokens[1], TokenKind::Ident(s) if s == "x"));
        assert_eq!(tokens[2], TokenKind::Assign);
        assert!(matches!(tokens[3], TokenKind::Integer(1)));
        assert_eq!(tokens[4], TokenKind::Newline);
        assert_eq!(tokens[5], TokenKind::Let);
    }

    #[test]
    fn test_comments_skipped() {
        let tokens = lex("let x = 1 // this is a comment\nlet y = 2");
        assert_eq!(tokens[0], TokenKind::Let);
        assert!(matches!(&tokens[1], TokenKind::Ident(s) if s == "x"));
        assert_eq!(tokens[2], TokenKind::Assign);
        assert!(matches!(tokens[3], TokenKind::Integer(1)));
        assert_eq!(tokens[4], TokenKind::Newline);
        assert_eq!(tokens[5], TokenKind::Let);
        assert!(matches!(&tokens[6], TokenKind::Ident(s) if s == "y"));
    }

    #[test]
    fn test_doc_comments() {
        let tokens = lex("/// This is a doc comment\nlet x = 1");
        assert!(matches!(&tokens[0], TokenKind::DocComment(s) if s == "This is a doc comment"));
        assert_eq!(tokens[1], TokenKind::Newline);
        assert_eq!(tokens[2], TokenKind::Let);
    }

    #[test]
    fn test_block_comments_skipped() {
        let tokens = lex("let x = 1 /* block comment */ let y = 2");
        assert_eq!(tokens[0], TokenKind::Let);
        assert!(matches!(&tokens[1], TokenKind::Ident(s) if s == "x"));
        assert_eq!(tokens[2], TokenKind::Assign);
        assert!(matches!(tokens[3], TokenKind::Integer(1)));
        assert_eq!(tokens[4], TokenKind::Let);
    }

    #[test]
    fn test_nested_block_comments() {
        let tokens = lex("/* outer /* inner */ outer */ let x = 1");
        assert_eq!(tokens[0], TokenKind::Let);
        assert!(matches!(&tokens[1], TokenKind::Ident(s) if s == "x"));
    }

    #[test]
    fn test_unicode_escape() {
        let tokens = lex(r#""\u{1F600}""#);
        assert!(matches!(&tokens[0], TokenKind::String(s) if s == "😀"));
    }

    #[test]
    fn test_unterminated_string() {
        let err = lex_err(r#""hello"#);
        assert!(err.contains("Unterminated string"));
    }

    #[test]
    fn test_unterminated_block_comment() {
        let err = lex_err("/* unterminated");
        assert!(err.contains("Unterminated block comment"));
    }

    #[test]
    fn test_unknown_escape() {
        let err = lex_err(r#""\q""#);
        assert!(err.contains("Unknown escape"));
    }

    #[test]
    fn test_full_program() {
        let source = r#"
let x :: Integer = 42
const PI :: Float = 3.14

fn add(a :: Integer, b :: Integer) :: Integer {
    return a + b
}

let result = add(x, 10)
output(result)
"#;
        let tokens = lex(source);
        // Should tokenize without errors
        assert!(!tokens.is_empty());
        assert_eq!(tokens.last().unwrap(), &TokenKind::Eof);
    }

    #[test]
    fn test_locations() {
        let lexer = Lexer::new("let x = 42\nlet y = 10");
        let tokens = lexer.tokenize().unwrap();
        
        // First 'let' at line 1, col 1
        assert_eq!(tokens[0].loc.line, 1);
        assert_eq!(tokens[0].loc.col, 1);
        
        // 'x' at line 1, col 5
        assert_eq!(tokens[1].loc.line, 1);
        assert_eq!(tokens[1].loc.col, 5);
        
        // Second 'let' at line 2, col 1
        assert_eq!(tokens[5].loc.line, 2);
        assert_eq!(tokens[5].loc.col, 1);
    }

    #[test]
    fn test_spans() {
        let lexer = Lexer::new("let x = 42");
        let tokens = lexer.tokenize().unwrap();
        
        // 'let' spans bytes 0-3
        assert_eq!(tokens[0].loc.span.start, 0);
        assert_eq!(tokens[0].loc.span.end, 3);
        
        // '42' spans bytes 8-10
        assert_eq!(tokens[3].loc.span.start, 8);
        assert_eq!(tokens[3].loc.span.end, 10);
    }
}
