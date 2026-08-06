// ============================================================================
// BangScript Parser
// Recursive descent parser that transforms a Token stream into an AST.
// ============================================================================

use crate::lexer::{Token, TokenKind, Loc};
use std::collections::HashMap;

// Re-export AST types for convenience
pub use crate::ast::*;

// ============================================================================
// Parser Error
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub struct ParseError {
    pub message: String,
    pub loc: Loc,
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Parse error at {}:{}: {}", self.loc.line, self.loc.col, self.message)
    }
}

impl std::error::Error for ParseError {}

// ============================================================================
// Parser
// ============================================================================

pub struct Parser {
    tokens: Vec<Token>,
    pos: usize,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, pos: 0 }
    }

    // ------------------------------------------------------------------------
    // Token access
    // ------------------------------------------------------------------------

    fn peek(&self, offset: usize) -> &Token {
        self.tokens.get(self.pos + offset)
            .unwrap_or_else(|| self.tokens.last().unwrap())
    }

    fn current(&self) -> &Token {
        self.peek(0)
    }

    fn advance(&mut self) -> &Token {
        let tok = &self.tokens[self.pos];
        if self.pos < self.tokens.len() - 1 {
            self.pos += 1;
        }
        tok
    }

    fn is_at_end(&self) -> bool {
        self.current().kind == TokenKind::Eof
    }

    fn check(&self, kind: &TokenKind) -> bool {
        std::mem::discriminant(&self.current().kind) == std::mem::discriminant(kind)
    }

    fn match_token(&mut self, kind: &TokenKind) -> bool {
        if self.check(kind) {
            self.advance();
            true
        } else {
            false
        }
    }

    fn expect(&mut self, kind: &TokenKind) -> Result<&Token, ParseError> {
        if self.check(kind) {
            Ok(self.advance())
        } else {
            Err(self.error(&format!("Expected {}, got {}", kind, self.current().kind)))
        }
    }

    fn error(&self, message: &str) -> ParseError {
        ParseError {
            message: message.to_string(),
            loc: self.current().loc.clone(),
        }
    }

    // ------------------------------------------------------------------------
    // Newline handling
    // ------------------------------------------------------------------------

    fn skip_newlines(&mut self) {
        while self.check(&TokenKind::Newline) {
            self.advance();
        }
    }

    fn consume_newlines(&mut self) -> Result<(), ParseError> {
        if !self.check(&TokenKind::Newline) && !self.is_at_end() {
            return Err(self.error("Expected newline or end of block"));
        }
        self.skip_newlines();
        Ok(())
    }

    // ------------------------------------------------------------------------
    // Type parsing
    // ------------------------------------------------------------------------

    fn parse_type(&mut self) -> Result<Type, ParseError> {
        self.skip_newlines();

        let token = self.current().clone();

        match &token.kind {
            TokenKind::Ident(name) => {
                let name = name.clone();
                self.advance();

                match name.as_str() {
                    "Integer" => Ok(Type::Integer),
                    "Float" => Ok(Type::Float),
                    "String" => Ok(Type::String),
                    "Bool" => Ok(Type::Bool),
                    "nil" => Ok(Type::Nil),
                    "Unknown" => Ok(Type::Unknown),
                    "List" => {
                        self.expect(&TokenKind::Lt)?;
                        let inner = self.parse_type()?;
                        self.expect(&TokenKind::Gt)?;
                        Ok(Type::List(Box::new(inner)))
                    }
                    "Map" => {
                        self.expect(&TokenKind::Lt)?;
                        let k = self.parse_type()?;
                        self.expect(&TokenKind::Comma)?;
                        let v = self.parse_type()?;
                        self.expect(&TokenKind::Gt)?;
                        Ok(Type::Map(Box::new(k), Box::new(v)))
                    }
                    "Set" => {
                        self.expect(&TokenKind::Lt)?;
                        let inner = self.parse_type()?;
                        self.expect(&TokenKind::Gt)?;
                        Ok(Type::Set(Box::new(inner)))
                    }
                    "Result" => {
                        self.expect(&TokenKind::Lt)?;
                        let t = self.parse_type()?;
                        self.expect(&TokenKind::Comma)?;
                        let e = self.parse_type()?;
                        self.expect(&TokenKind::Gt)?;
                        Ok(Type::Result(Box::new(t), Box::new(e)))
                    }
                    "Maybe" => {
                        self.expect(&TokenKind::Lt)?;
                        let inner = self.parse_type()?;
                        self.expect(&TokenKind::Gt)?;
                        Ok(Type::Maybe(Box::new(inner)))
                    }
                    _ => Ok(Type::Named(name)),
                }
            }
            TokenKind::LParen => {
                // Function type: (T, U) -> V
                self.advance();
                let mut params = Vec::new();
                if !self.check(&TokenKind::RParen) {
                    params.push(self.parse_type()?);
                    while self.match_token(&TokenKind::Comma) {
                        params.push(self.parse_type()?);
                    }
                }
                self.expect(&TokenKind::RParen)?;
                self.expect(&TokenKind::Arrow)?;
                let ret = self.parse_type()?;
                Ok(Type::Function(params, Box::new(ret)))
            }
            _ => Err(self.error(&format!("Expected type, got {}", token.kind))),
        }
    }

    // ------------------------------------------------------------------------
    // Pattern parsing
    // ------------------------------------------------------------------------

    fn parse_pattern(&mut self) -> Result<Pattern, ParseError> {
        self.skip_newlines();

        let pat = match &self.current().kind {
            TokenKind::Ident(name) if name == "_" => {
                self.advance();
                Pattern::Wildcard
            }
            TokenKind::Integer(n) => {
                let n = *n;
                self.advance();
                Pattern::Literal(Expr::Integer(n))
            }
            TokenKind::Float(f) => {
                let f = *f;
                self.advance();
                Pattern::Literal(Expr::Float(f))
            }
            TokenKind::String(s) => {
                let s = s.clone();
                self.advance();
                Pattern::Literal(Expr::String(s))
            }
            TokenKind::Bool(b) => {
                let b = *b;
                self.advance();
                Pattern::Literal(Expr::Bool(b))
            }
            TokenKind::NilKw => {
                self.advance();
                Pattern::Literal(Expr::Nil)
            }
            TokenKind::Ident(name) => {
                let name = name.clone();
                self.advance();
                Pattern::Variable(name)
            }
            TokenKind::LBrace => {
                // Struct pattern: { x, y } or { x: 0, .. }
                self.advance();
                let mut fields = Vec::new();
                while !self.check(&TokenKind::RBrace) && !self.is_at_end() {
                    let field_name = match &self.current().kind {
                        TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                        _ => return Err(self.error("Expected field name in pattern")),
                    };

                    let field_pat = if self.match_token(&TokenKind::Colon) {
                        self.parse_pattern()?
                    } else {
                        Pattern::Variable(field_name.clone())
                    };

                    fields.push((field_name, field_pat));

                    if self.match_token(&TokenKind::Comma) {
                        continue;
                    } else if self.check(&TokenKind::DotDot) {
                        self.advance();
                        fields.push(("..".to_string(), Pattern::Wildcard));
                        break;
                    } else {
                        break;
                    }
                }
                self.expect(&TokenKind::RBrace)?;
                Pattern::Struct(fields)
            }
            TokenKind::LBracket => {
                // List pattern: [a, b, ..rest]
                self.advance();
                let mut items = Vec::new();
                while !self.check(&TokenKind::RBracket) && !self.is_at_end() {
                    items.push(self.parse_pattern()?);
                    if !self.match_token(&TokenKind::Comma) {
                        break;
                    }
                }
                self.expect(&TokenKind::RBracket)?;
                Pattern::List(items)
            }
            _ => return Err(self.error(&format!("Unexpected pattern: {}", self.current().kind))),
        };

        // Guard: `if expr`
        if self.match_token(&TokenKind::If) {
            let cond = self.parse_expr()?;
            return Ok(Pattern::Guarded {
                pattern: Box::new(pat),
                cond: Box::new(cond),
            });
        }

        Ok(pat)
    }

    // ------------------------------------------------------------------------
    // Expression parsing (precedence climbing)
    // ------------------------------------------------------------------------

    pub fn parse_expr(&mut self) -> Result<Expr, ParseError> {
        self.parse_assignment()
    }

    fn parse_assignment(&mut self) -> Result<Expr, ParseError> {
        let expr = self.parse_or()?;

        if self.match_token(&TokenKind::Assign) {
            let value = self.parse_assignment()?;
            return Ok(Expr::Assign {
                target: Box::new(expr),
                value: Box::new(value),
            });
        }

        Ok(expr)
    }

    fn parse_or(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_and()?;

        while self.match_token(&TokenKind::Or) {
            let right = self.parse_and()?;
            left = Expr::Binary {
                op: BinOp::Or,
                left: Box::new(left),
                right: Box::new(right),
            };
        }

        Ok(left)
    }

    fn parse_and(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_equality()?;

        while self.match_token(&TokenKind::And) {
            let right = self.parse_equality()?;
            left = Expr::Binary {
                op: BinOp::And,
                left: Box::new(left),
                right: Box::new(right),
            };
        }

        Ok(left)
    }

    fn parse_equality(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_comparison()?;

        loop {
            if self.match_token(&TokenKind::Eq) {
                let right = self.parse_comparison()?;
                left = Expr::Binary { op: BinOp::Eq, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Neq) {
                let right = self.parse_comparison()?;
                left = Expr::Binary { op: BinOp::Neq, left: Box::new(left), right: Box::new(right) };
            } else {
                break;
            }
        }

        Ok(left)
    }

    fn parse_comparison(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_range()?;

        loop {
            if self.match_token(&TokenKind::Lt) {
                let right = self.parse_range()?;
                left = Expr::Binary { op: BinOp::Lt, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Gt) {
                let right = self.parse_range()?;
                left = Expr::Binary { op: BinOp::Gt, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Leq) {
                let right = self.parse_range()?;
                left = Expr::Binary { op: BinOp::Leq, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Geq) {
                let right = self.parse_range()?;
                left = Expr::Binary { op: BinOp::Geq, left: Box::new(left), right: Box::new(right) };
            } else {
                break;
            }
        }

        Ok(left)
    }

    fn parse_range(&mut self) -> Result<Expr, ParseError> {
        let left = self.parse_term()?;

        if self.match_token(&TokenKind::DotDot) {
            let right = self.parse_term()?;
            Ok(Expr::Range {
                start: Box::new(left),
                end: Box::new(right),
                inclusive: false,
            })
        } else if self.match_token(&TokenKind::DotDotEq) {
            let right = self.parse_term()?;
            Ok(Expr::Range {
                start: Box::new(left),
                end: Box::new(right),
                inclusive: true,
            })
        } else {
            Ok(left)
        }
    }

    fn parse_term(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_factor()?;

        loop {
            if self.match_token(&TokenKind::Plus) {
                let right = self.parse_factor()?;
                left = Expr::Binary { op: BinOp::Add, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Minus) {
                let right = self.parse_factor()?;
                left = Expr::Binary { op: BinOp::Sub, left: Box::new(left), right: Box::new(right) };
            } else {
                break;
            }
        }

        Ok(left)
    }

    fn parse_factor(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_unary()?;

        loop {
            if self.match_token(&TokenKind::Star) {
                let right = self.parse_unary()?;
                left = Expr::Binary { op: BinOp::Mul, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Slash) {
                let right = self.parse_unary()?;
                left = Expr::Binary { op: BinOp::Div, left: Box::new(left), right: Box::new(right) };
            } else if self.match_token(&TokenKind::Percent) {
                let right = self.parse_unary()?;
                left = Expr::Binary { op: BinOp::Mod, left: Box::new(left), right: Box::new(right) };
            } else {
                break;
            }
        }

        Ok(left)
    }

    fn parse_unary(&mut self) -> Result<Expr, ParseError> {
        if self.match_token(&TokenKind::Minus) {
            let expr = self.parse_unary()?;
            Ok(Expr::Unary { op: UnOp::Neg, expr: Box::new(expr) })
        } else if self.match_token(&TokenKind::Not) {
            let expr = self.parse_unary()?;
            Ok(Expr::Unary { op: UnOp::Not, expr: Box::new(expr) })
        } else {
            self.parse_rbt()
        }
    }

    fn parse_rbt(&mut self) -> Result<Expr, ParseError> {
        // RBT operators: !T, !~T, ?T
        if self.match_token(&TokenKind::Bang) {
            let typ = self.parse_type()?;
            let expr = self.parse_postfix()?;
            return Ok(Expr::RbtProve { typ, expr: Box::new(expr) });
        }

        if self.match_token(&TokenKind::BangTilde) {
            let typ = self.parse_type()?;
            let expr = self.parse_postfix()?;
            return Ok(Expr::RbtMask { typ, expr: Box::new(expr) });
        }

        if self.match_token(&TokenKind::Question) {
            let typ = self.parse_type()?;
            let expr = self.parse_postfix()?;
            return Ok(Expr::RbtQuery { typ, expr: Box::new(expr) });
        }

        self.parse_postfix()
    }

    fn parse_postfix(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_primary()?;

        loop {
            if self.match_token(&TokenKind::LParen) {
                // Function call
                let mut args = Vec::new();
                if !self.check(&TokenKind::RParen) {
                    args.push(self.parse_expr()?);
                    while self.match_token(&TokenKind::Comma) {
                        args.push(self.parse_expr()?);
                    }
                }
                self.expect(&TokenKind::RParen)?;
                expr = Expr::Call {
                    func: Box::new(expr),
                    args,
                };
            } else if self.match_token(&TokenKind::LBracket) {
                // Indexing: expr[index]
                let index = self.parse_expr()?;
                self.expect(&TokenKind::RBracket)?;
                expr = Expr::Index {
                    expr: Box::new(expr),
                    index: Box::new(index),
                };
            } else if self.match_token(&TokenKind::Dot) {
                // Member access: expr.name
                let name = match &self.current().kind {
                    TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                    _ => return Err(self.error("Expected identifier after '.'")),
                };
                expr = Expr::Member {
                    expr: Box::new(expr),
                    name,
                };
            } else {
                break;
            }
        }

        Ok(expr)
    }

    fn parse_primary(&mut self) -> Result<Expr, ParseError> {
        self.skip_newlines();

        let token = self.current().clone();

        match &token.kind {
            TokenKind::Integer(n) => {
                let n = *n;
                self.advance();
                Ok(Expr::Integer(n))
            }
            TokenKind::Float(f) => {
                let f = *f;
                self.advance();
                Ok(Expr::Float(f))
            }
            TokenKind::String(s) => {
                let s = s.clone();
                self.advance();
                Ok(Expr::String(s))
            }
            TokenKind::RawString(s) => {
                let s = s.clone();
                self.advance();
                Ok(Expr::RawString(s))
            }
            TokenKind::Bool(b) => {
                let b = *b;
                self.advance();
                Ok(Expr::Bool(b))
            }
            TokenKind::NilKw => {
                self.advance();
                Ok(Expr::Nil)
            }
            TokenKind::Ident(name) => {
                let name = name.clone();
                self.advance();
                Ok(Expr::Ident(name))
            }
            TokenKind::LParen => {
                self.advance();
                let expr = self.parse_expr()?;
                self.expect(&TokenKind::RParen)?;
                Ok(expr)
            }
            TokenKind::LBracket => {
                // List literal: [1, 2, 3]
                self.advance();
                let mut items = Vec::new();
                if !self.check(&TokenKind::RBracket) {
                    items.push(self.parse_expr()?);
                    while self.match_token(&TokenKind::Comma) {
                        items.push(self.parse_expr()?);
                    }
                }
                self.expect(&TokenKind::RBracket)?;
                Ok(Expr::List(items))
            }
            TokenKind::LBrace => {
                // Map literal: {a: 1, b: 2}
                self.advance();
                let mut items = Vec::new();
                if !self.check(&TokenKind::RBrace) {
                    let key = match &self.current().kind {
                        TokenKind::Ident(k) => { let k = k.clone(); self.advance(); k }
                        TokenKind::String(k) => { let k = k.clone(); self.advance(); k }
                        _ => return Err(self.error("Expected map key")),
                    };
                    self.expect(&TokenKind::Colon)?;
                    let value = self.parse_expr()?;
                    items.push((key, value));
                    while self.match_token(&TokenKind::Comma) {
                        let key = match &self.current().kind {
                            TokenKind::Ident(k) => { let k = k.clone(); self.advance(); k }
                            TokenKind::String(k) => { let k = k.clone(); self.advance(); k }
                            _ => return Err(self.error("Expected map key")),
                        };
                        self.expect(&TokenKind::Colon)?;
                        let value = self.parse_expr()?;
                        items.push((key, value));
                    }
                }
                self.expect(&TokenKind::RBrace)?;
                Ok(Expr::Map(items))
            }
            TokenKind::If => self.parse_if_expr(),
            TokenKind::Match => self.parse_match_expr(),
            TokenKind::Ld => self.parse_lambda_expr(),
            TokenKind::Fn => self.parse_fn_expr(),
            _ => Err(self.error(&format!("Unexpected token in expression: {}", token.kind))),
        }
    }

    // ------------------------------------------------------------------------
    // Complex expressions
    // ------------------------------------------------------------------------

    fn parse_if_expr(&mut self) -> Result<Expr, ParseError> {
        self.expect(&TokenKind::If)?;
        let cond = self.parse_expr()?;
        self.expect(&TokenKind::LBrace)?;
        let then_branch = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        let else_branch = if self.match_token(&TokenKind::Else) {
            self.skip_newlines();
            if self.check(&TokenKind::If) {
                // else if
                let else_if = self.parse_if_expr()?;
                Some(vec![Stmt::Expr(else_if)])
            } else {
                self.expect(&TokenKind::LBrace)?;
                let block = self.parse_block()?;
                self.expect(&TokenKind::RBrace)?;
                Some(block)
            }
        } else {
            None
        };

        Ok(Expr::If {
            cond: Box::new(cond),
            then_branch,
            else_branch,
        })
    }

    fn parse_match_expr(&mut self) -> Result<Expr, ParseError> {
        self.expect(&TokenKind::Match)?;
        let expr = self.parse_expr()?;
        self.expect(&TokenKind::LBrace)?;

        let mut arms = Vec::new();
        while !self.check(&TokenKind::RBrace) && !self.is_at_end() {
            let pat = self.parse_pattern()?;
            self.expect(&TokenKind::FatArrow)?;
            self.expect(&TokenKind::LBrace)?;
            let body = self.parse_block()?;
            self.expect(&TokenKind::RBrace)?;
            arms.push((pat, body));

            if self.match_token(&TokenKind::Comma) {
                continue;
            } else if self.check(&TokenKind::RBrace) {
                break;
            } else {
                return Err(self.error("Expected ',' or '}' after match arm"));
            }
        }

        self.expect(&TokenKind::RBrace)?;
        Ok(Expr::Match {
            expr: Box::new(expr),
            arms,
        })
    }

    fn parse_lambda_expr(&mut self) -> Result<Expr, ParseError> {
        self.expect(&TokenKind::Ld)?;

        // Optional name
        let name = if let TokenKind::Ident(n) = &self.current().kind {
            let n = n.clone();
            self.advance();
            Some(n)
        } else {
            None
        };

        self.expect(&TokenKind::LParen)?;
        let params = self.parse_param_list()?;
        self.expect(&TokenKind::RParen)?;

        // Default values
        let mut defaults = Vec::new();
        for i in 0..params.len() {
            if self.match_token(&TokenKind::Assign) {
                let default = self.parse_expr()?;
                defaults.push((i, default));
            }
        }

        self.expect(&TokenKind::Arrow)?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        Ok(Expr::Lambda {
            name,
            params,
            defaults,
            body,
        })
    }

    fn parse_fn_expr(&mut self) -> Result<Expr, ParseError> {
        // Anonymous function: fn() { ... }
        self.expect(&TokenKind::Fn)?;
        self.expect(&TokenKind::LParen)?;
        let params = self.parse_param_list()?;
        self.expect(&TokenKind::RParen)?;

        let ret_type = if self.match_token(&TokenKind::DoubleColon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        Ok(Expr::FnExpr {
            params,
            ret_type,
            body,
        })
    }

    // ------------------------------------------------------------------------
    // Statement parsing
    // ------------------------------------------------------------------------

    pub fn parse_stmt(&mut self) -> Result<Stmt, ParseError> {
        self.skip_newlines();

        match &self.current().kind {
            TokenKind::Let => self.parse_let_stmt(),
            TokenKind::Const => self.parse_const_stmt(),
            TokenKind::Fn => self.parse_fn_decl(),
            TokenKind::Ld => self.parse_ld_decl(),
            TokenKind::Type => self.parse_type_decl(),
            TokenKind::Import => self.parse_import(),
            TokenKind::Export => self.parse_export(),
            TokenKind::Return => self.parse_return(),
            TokenKind::While => self.parse_while(),
            TokenKind::For => self.parse_for(),
            TokenKind::Break => { self.advance(); Ok(Stmt::Break) }
            TokenKind::Continue => { self.advance(); Ok(Stmt::Continue) }
            TokenKind::Unsafe => self.parse_unsafe(),
            TokenKind::Spawn => self.parse_spawn(),
            TokenKind::Try => self.parse_try(),
            TokenKind::Match => {
                let expr = self.parse_match_expr()?;
                Ok(Stmt::Expr(expr))
            }
            _ => {
                let expr = self.parse_expr()?;
                Ok(Stmt::Expr(expr))
            }
        }
    }

    fn parse_let_stmt(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Let)?;

        let mutable = self.match_token(&TokenKind::Mut);

        let name = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected variable name after 'let'")),
        };

        let typ = if self.match_token(&TokenKind::DoubleColon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect(&TokenKind::Assign)?;
        let value = self.parse_expr()?;

        Ok(Stmt::Let { name, typ, mutable, value })
    }

    fn parse_const_stmt(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Const)?;

        let name = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected constant name after 'const'")),
        };

        let typ = if self.match_token(&TokenKind::DoubleColon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect(&TokenKind::Assign)?;
        let value = self.parse_expr()?;

        Ok(Stmt::Const { name, typ, value })
    }

    fn parse_fn_decl(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Fn)?;

        let name = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected function name after 'fn'")),
        };

        self.expect(&TokenKind::LParen)?;
        let params = self.parse_param_list()?;
        self.expect(&TokenKind::RParen)?;

        let ret_type = if self.match_token(&TokenKind::DoubleColon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        Ok(Stmt::FnDecl { name, params, ret_type, body })
    }

    fn parse_ld_decl(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Ld)?;

        let name = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected lambda name after 'ld'")),
        };

        self.expect(&TokenKind::LParen)?;
        let params = self.parse_param_list()?;
        self.expect(&TokenKind::RParen)?;

        // Default values
        let mut defaults = Vec::new();
        for i in 0..params.len() {
            if self.match_token(&TokenKind::Assign) {
                let default = self.parse_expr()?;
                defaults.push((i, default));
            }
        }

        self.expect(&TokenKind::Arrow)?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        Ok(Stmt::LdDecl { name, params, defaults, body })
    }

    fn parse_param_list(&mut self) -> Result<Vec<(String, Option<Type>)>, ParseError> {
        let mut params = Vec::new();

        if !self.check(&TokenKind::RParen) {
            // Variadic: ..name
            if self.match_token(&TokenKind::DotDot) {
                let name = match &self.current().kind {
                    TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                    _ => return Err(self.error("Expected parameter name after '..'")),
                };
                params.push((format!("..{}", name), Some(Type::List(Box::new(Type::Unknown)))));
                return Ok(params);
            }

            let pname = match &self.current().kind {
                TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                _ => return Err(self.error("Expected parameter name")),
            };

            let ptyp = if self.match_token(&TokenKind::DoubleColon) {
                Some(self.parse_type()?)
            } else {
                None
            };

            params.push((pname, ptyp));

            while self.match_token(&TokenKind::Comma) {
                if self.check(&TokenKind::RParen) {
                    break;
                }

                let pname = match &self.current().kind {
                    TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                    _ => return Err(self.error("Expected parameter name")),
                };

                let ptyp = if self.match_token(&TokenKind::DoubleColon) {
                    Some(self.parse_type()?)
                } else {
                    None
                };

                params.push((pname, ptyp));
            }
        }

        Ok(params)
    }

    fn parse_type_decl(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Type)?;

        let name = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected type name after 'type'")),
        };

        self.expect(&TokenKind::Assign)?;
        let typ = self.parse_type()?;

        Ok(Stmt::TypeDecl { name, typ })
    }

    fn parse_import(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Import)?;

        let mut items = Vec::new();
        let mut aliases = HashMap::new();

        if self.match_token(&TokenKind::LBrace) {
            loop {
                let item = match &self.current().kind {
                    TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                    _ => return Err(self.error("Expected import item")),
                };

                if self.match_token(&TokenKind::As) {
                    let alias = match &self.current().kind {
                        TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                        _ => return Err(self.error("Expected alias after 'as'")),
                    };
                    aliases.insert(item.clone(), alias);
                }

                items.push(item);

                if !self.match_token(&TokenKind::Comma) {
                    break;
                }
            }
            self.expect(&TokenKind::RBrace)?;
        } else if let TokenKind::Ident(n) = &self.current().kind {
            let n = n.clone();
            self.advance();
            items.push(n);
        } else if self.match_token(&TokenKind::Star) {
            items.push("*".to_string());
        }

        self.expect(&TokenKind::From)?;

        let source = match &self.current().kind {
            TokenKind::String(s) => { let s = s.clone(); self.advance(); s }
            _ => return Err(self.error("Expected import source string")),
        };

        Ok(Stmt::Import { items, aliases, source })
    }

    fn parse_export(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Export)?;
        let stmt = self.parse_stmt()?;
        Ok(Stmt::Export(Box::new(stmt)))
    }

    fn parse_return(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Return)?;

        let value = if self.check(&TokenKind::Newline)
            || self.check(&TokenKind::RBrace)
            || self.check(&TokenKind::Eof)
            || self.check(&TokenKind::Semi)
        {
            None
        } else {
            Some(self.parse_expr()?)
        };

        Ok(Stmt::Return(value))
    }

    fn parse_while(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::While)?;
        let cond = self.parse_expr()?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;
        Ok(Stmt::While { cond, body })
    }

    fn parse_for(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::For)?;

        let var = match &self.current().kind {
            TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
            _ => return Err(self.error("Expected loop variable after 'for'")),
        };

        self.expect(&TokenKind::In)?;
        let iterable = self.parse_expr()?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        Ok(Stmt::For { var, iterable, body })
    }

    fn parse_unsafe(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Unsafe)?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;
        Ok(Stmt::Unsafe(body))
    }

    fn parse_spawn(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Spawn)?;
        let expr = self.parse_expr()?;
        Ok(Stmt::Spawn(Box::new(expr)))
    }

    fn parse_try(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&TokenKind::Try)?;
        self.expect(&TokenKind::LBrace)?;
        let body = self.parse_block()?;
        self.expect(&TokenKind::RBrace)?;

        let mut catches = Vec::new();
        while self.match_token(&TokenKind::Catch) {
            let error_var = match &self.current().kind {
                TokenKind::Ident(n) => { let n = n.clone(); self.advance(); n }
                _ => return Err(self.error("Expected error variable after 'catch'")),
            };

            let error_type = if self.match_token(&TokenKind::DoubleColon) {
                Some(self.parse_type()?)
            } else {
                None
            };

            self.expect(&TokenKind::LBrace)?;
            let catch_body = self.parse_block()?;
            self.expect(&TokenKind::RBrace)?;

            catches.push((error_var, error_type, catch_body));
        }

        Ok(Stmt::Try { body, catches })
    }

    // ------------------------------------------------------------------------
    // Block parsing
    // ------------------------------------------------------------------------

    fn parse_block(&mut self) -> Result<Vec<Stmt>, ParseError> {
        let mut stmts = Vec::new();

        self.skip_newlines();

        while !self.check(&TokenKind::RBrace) && !self.is_at_end() {
            stmts.push(self.parse_stmt()?);
            self.skip_newlines();
        }

        Ok(stmts)
    }

    // ------------------------------------------------------------------------
    // Top-level parsing
    // ------------------------------------------------------------------------

    pub fn parse(&mut self) -> Result<Vec<Stmt>, ParseError> {
        let mut stmts = Vec::new();

        self.skip_newlines();

        while !self.is_at_end() {
            stmts.push(self.parse_stmt()?);
            self.skip_newlines();
        }

        Ok(stmts)
    }
}

// ============================================================================
// Convenience function
// ============================================================================

pub fn parse(tokens: Vec<Token>) -> Result<Vec<Stmt>, ParseError> {
    let mut parser = Parser::new(tokens);
    parser.parse()
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::Lexer;

    fn parse_source(source: &str) -> Vec<Stmt> {
        let lexer = Lexer::new(source);
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        parser.parse().unwrap()
    }

    fn parse_expr_source(source: &str) -> Expr {
        let lexer = Lexer::new(source);
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        parser.parse_expr().unwrap()
    }

    #[test]
    fn test_let_simple() {
        let stmts = parse_source("let x = 42");
        assert_eq!(stmts.len(), 1);
        match &stmts[0] {
            Stmt::Let { name, typ, mutable, value } => {
                assert_eq!(name, "x");
                assert_eq!(*mutable, false);
                assert!(matches!(value, Expr::Integer(42)));
            }
            _ => panic!("Expected let statement"),
        }
    }

    #[test]
    fn test_let_typed() {
        let stmts = parse_source("let x :: Integer = 42");
        match &stmts[0] {
            Stmt::Let { name, typ, .. } => {
                assert_eq!(name, "x");
                assert_eq!(*typ, Some(Type::Integer));
            }
            _ => panic!("Expected let statement"),
        }
    }

    #[test]
    fn test_let_mut() {
        let stmts = parse_source("let mut x = 42");
        match &stmts[0] {
            Stmt::Let { mutable, .. } => {
                assert_eq!(*mutable, true);
            }
            _ => panic!("Expected let statement"),
        }
    }

    #[test]
    fn test_const() {
        let stmts = parse_source("const PI :: Float = 3.14");
        match &stmts[0] {
            Stmt::Const { name, typ, value } => {
                assert_eq!(name, "PI");
                assert_eq!(*typ, Some(Type::Float));
                assert!(matches!(value, Expr::Float(f) if (f - 3.14).abs() < 0.001));
            }
            _ => panic!("Expected const statement"),
        }
    }

    #[test]
    fn test_fn_decl() {
        let stmts = parse_source(r#"
fn add(a :: Integer, b :: Integer) :: Integer {
    return a + b
}
"#);
        match &stmts[0] {
            Stmt::FnDecl { name, params, ret_type, body } => {
                assert_eq!(name, "add");
                assert_eq!(params.len(), 2);
                assert_eq!(params[0], ("a".to_string(), Some(Type::Integer)));
                assert_eq!(params[1], ("b".to_string(), Some(Type::Integer)));
                assert_eq!(*ret_type, Some(Type::Integer));
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected fn declaration"),
        }
    }

    #[test]
    fn test_ld_decl() {
        let stmts = parse_source(r#"
ld double(x :: Integer) -> {
    return x * 2
}
"#);
        match &stmts[0] {
            Stmt::LdDecl { name, params, body } => {
                assert_eq!(name, "double");
                assert_eq!(params[0], ("x".to_string(), Some(Type::Integer)));
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected ld declaration"),
        }
    }

    #[test]
    fn test_if_expr() {
        let expr = parse_expr_source("if x > 0 { 1 } else { 0 }");
        match expr {
            Expr::If { cond, then_branch, else_branch } => {
                assert!(matches!(cond.as_ref(), Expr::Binary { op: BinOp::Gt, .. }));
                assert_eq!(then_branch.len(), 1);
                assert!(else_branch.is_some());
            }
            _ => panic!("Expected if expression"),
        }
    }

    #[test]
    fn test_if_no_else() {
        let expr = parse_expr_source("if x > 0 { 1 }");
        match expr {
            Expr::If { else_branch, .. } => {
                assert!(else_branch.is_none());
            }
            _ => panic!("Expected if expression"),
        }
    }

    #[test]
    fn test_match() {
        let expr = parse_expr_source(r#"
match x {
    0 => { "zero" }
    1 => { "one" }
    _ => { "other" }
}
"#);
        match expr {
            Expr::Match { expr, arms } => {
                assert!(matches!(expr.as_ref(), Expr::Ident(name) if name == "x"));
                assert_eq!(arms.len(), 3);
            }
            _ => panic!("Expected match expression"),
        }
    }

    #[test]
    fn test_lambda() {
        let expr = parse_expr_source("ld(x) -> { x + 1 }");
        match expr {
            Expr::Lambda { name, params, body, .. } => {
                assert!(name.is_none());
                assert_eq!(params.len(), 1);
                assert_eq!(params[0].0, "x");
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected lambda expression"),
        }
    }

    #[test]
    fn test_named_lambda() {
        let expr = parse_expr_source("ld double(x) -> { x * 2 }");
        match expr {
            Expr::Lambda { name, .. } => {
                assert_eq!(name, Some("double".to_string()));
            }
            _ => panic!("Expected named lambda"),
        }
    }

    #[test]
    fn test_list_literal() {
        let expr = parse_expr_source("[1, 2, 3]");
        match expr {
            Expr::List(items) => {
                assert_eq!(items.len(), 3);
                assert!(matches!(items[0], Expr::Integer(1)));
            }
            _ => panic!("Expected list literal"),
        }
    }

    #[test]
    fn test_map_literal() {
        let expr = parse_expr_source(r#"{a: 1, b: 2}"#);
        match expr {
            Expr::Map(items) => {
                assert_eq!(items.len(), 2);
                assert_eq!(items[0].0, "a");
                assert!(matches!(items[0].1, Expr::Integer(1)));
            }
            _ => panic!("Expected map literal"),
        }
    }

    #[test]
    fn test_call() {
        let expr = parse_expr_source("add(1, 2)");
        match expr {
            Expr::Call { func, args } => {
                assert!(matches!(func.as_ref(), Expr::Ident(name) if name == "add"));
                assert_eq!(args.len(), 2);
            }
            _ => panic!("Expected call expression"),
        }
    }

    #[test]
    fn test_index() {
        let expr = parse_expr_source("arr[0]");
        match expr {
            Expr::Index { expr, index } => {
                assert!(matches!(expr.as_ref(), Expr::Ident(name) if name == "arr"));
                assert!(matches!(index.as_ref(), Expr::Integer(0)));
            }
            _ => panic!("Expected index expression"),
        }
    }

    #[test]
    fn test_member() {
        let expr = parse_expr_source("obj.field");
        match expr {
            Expr::Member { expr, name } => {
                assert!(matches!(expr.as_ref(), Expr::Ident(n) if n == "obj"));
                assert_eq!(name, "field");
            }
            _ => panic!("Expected member expression"),
        }
    }

    #[test]
    fn test_rbt_prove() {
        let expr = parse_expr_source("!Integer x");
        match expr {
            Expr::RbtProve { typ, expr } => {
                assert_eq!(*typ, Type::Integer);
                assert!(matches!(expr.as_ref(), Expr::Ident(name) if name == "x"));
            }
            _ => panic!("Expected RBT prove expression"),
        }
    }

    #[test]
    fn test_rbt_mask() {
        let expr = parse_expr_source("!~Integer x");
        match expr {
            Expr::RbtMask { typ, .. } => {
                assert_eq!(*typ, Type::Integer);
            }
            _ => panic!("Expected RBT mask expression"),
        }
    }

    #[test]
    fn test_rbt_query() {
        let expr = parse_expr_source("?Integer x");
        match expr {
            Expr::RbtQuery { typ, .. } => {
                assert_eq!(*typ, Type::Integer);
            }
            _ => panic!("Expected RBT query expression"),
        }
    }

    #[test]
    fn test_binary_ops() {
        let expr = parse_expr_source("1 + 2 * 3");
        match expr {
            Expr::Binary { op: BinOp::Add, left, right } => {
                assert!(matches!(left.as_ref(), Expr::Integer(1)));
                assert!(matches!(right.as_ref(), Expr::Binary { op: BinOp::Mul, .. }));
            }
            _ => panic!("Expected binary expression with correct precedence"),
        }
    }

    #[test]
    fn test_while() {
        let stmts = parse_source(r#"
while x > 0 {
    x = x - 1
}
"#);
        match &stmts[0] {
            Stmt::While { cond, body } => {
                assert!(matches!(cond, Expr::Binary { op: BinOp::Gt, .. }));
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected while statement"),
        }
    }

    #[test]
    fn test_for() {
        let stmts = parse_source(r#"
for i in 0..10 {
    output(i)
}
"#);
        match &stmts[0] {
            Stmt::For { var, iterable, body } => {
                assert_eq!(var, "i");
                assert!(matches!(iterable, Expr::Range { .. }));
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected for statement"),
        }
    }

    #[test]
    fn test_type_decl() {
        let stmts = parse_source("type Point = { x :: Float, y :: Float }");
        match &stmts[0] {
            Stmt::TypeDecl { name, typ } => {
                assert_eq!(name, "Point");
                assert!(matches!(typ, Type::Map(_, _)));
            }
            _ => panic!("Expected type declaration"),
        }
    }

    #[test]
    fn test_import() {
        let stmts = parse_source(r#"import { map, filter } from "std/list""#);
        match &stmts[0] {
            Stmt::Import { items, source, .. } => {
                assert_eq!(items, vec!["map", "filter"]);
                assert_eq!(source, "std/list");
            }
            _ => panic!("Expected import statement"),
        }
    }

    #[test]
    fn test_export() {
        let stmts = parse_source("export fn add(a, b) { return a + b }");
        match &stmts[0] {
            Stmt::Export(stmt) => {
                assert!(matches!(stmt.as_ref(), Stmt::FnDecl { name, .. } if name == "add"));
            }
            _ => panic!("Expected export statement"),
        }
    }

    #[test]
    fn test_try_catch() {
        let stmts = parse_source(r#"
try {
    risky()
} catch e :: Error {
    output(e)
}
"#);
        match &stmts[0] {
            Stmt::Try { body, catches } => {
                assert_eq!(body.len(), 1);
                assert_eq!(catches.len(), 1);
                assert_eq!(catches[0].0, "e");
                assert_eq!(catches[0].1, Some(Type::Named("Error".to_string())));
            }
            _ => panic!("Expected try statement"),
        }
    }

    #[test]
    fn test_unsafe() {
        let stmts = parse_source(r#"
unsafe {
    let ptr = malloc(1024)
}
"#);
        match &stmts[0] {
            Stmt::Unsafe(body) => {
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected unsafe block"),
        }
    }

    #[test]
    fn test_spawn() {
        let stmts = parse_source("spawn fn() { output(1) }");
        match &stmts[0] {
            Stmt::Spawn(expr) => {
                assert!(matches!(expr.as_ref(), Expr::FnExpr { .. }));
            }
            _ => panic!("Expected spawn statement"),
        }
    }

    #[test]
    fn test_range() {
        let expr = parse_expr_source("0..10");
        match expr {
            Expr::Range { start, end, inclusive } => {
                assert!(matches!(start.as_ref(), Expr::Integer(0)));
                assert!(matches!(end.as_ref(), Expr::Integer(10)));
                assert_eq!(*inclusive, false);
            }
            _ => panic!("Expected range expression"),
        }
    }

    #[test]
    fn test_range_inclusive() {
        let expr = parse_expr_source("0..=10");
        match expr {
            Expr::Range { inclusive, .. } => {
                assert_eq!(*inclusive, true);
            }
            _ => panic!("Expected inclusive range"),
        }
    }

    #[test]
    fn test_full_program() {
        let stmts = parse_source(r#"
let x :: Integer = 42
const PI :: Float = 3.14

fn add(a :: Integer, b :: Integer) :: Integer {
    return a + b
}

ld double(x) -> {
    return x * 2
}

let result = add(x, 10)

if result > 50 {
    output(result)
} else {
    output(0)
}

for i in 0..10 {
    output(i)
}

let nums = [1, 2, 3]
let first = nums[0]

let user = {
    name: "BangScript",
    version: 1
}
"#);
        assert_eq!(stmts.len(), 10);
    }

    #[test]
    fn test_error_unexpected_token() {
        let lexer = Lexer::new("let 42 = x");
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let err = parser.parse().unwrap_err();
        assert!(err.message.contains("Expected variable name"));
    }

    #[test]
    fn test_error_missing_brace() {
        let lexer = Lexer::new("fn add() { return 1");
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let err = parser.parse().unwrap_err();
        assert!(err.message.contains("Expected") || err.message.contains("}"));
    }
}
