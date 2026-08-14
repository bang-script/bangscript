#include "parser.h"

#include <stdexcept>
#include <utility>

namespace bang {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> stmts;
    skip_newlines();
    while (!at_end()) {
        stmts.push_back(parse_statement());
        skip_newlines();
    }
    return stmts;
}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::at_end() const {
    return peek().type == TokenType::Eof;
}

bool Parser::check(TokenType type) const {
    if (at_end()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::advance() {
    Token tok = tokens_[current_++];
    switch (tok.type) {
        case TokenType::LParen:
        case TokenType::LBracket:
        case TokenType::LBrace:
            depth_++;
            break;
        case TokenType::RParen:
        case TokenType::RBracket:
        case TokenType::RBrace:
            depth_--;
            break;
        default:
            break;
    }
    return tok;
}

void Parser::skip_newlines() {
    while (peek().type == TokenType::Newline) advance();
}

[[noreturn]] void Parser::error(const Token& tok, const std::string& msg) {
    std::string out = "parse error at " + std::to_string(tok.line) + ":"
                    + std::to_string(tok.column) + ": " + msg;
    errors_.push_back(out);
    throw std::runtime_error(out);
}

ExprPtr Parser::parse_expression(int min_bp) {
    ExprPtr lhs = parse_prefix();
    while (true) {
        if (peek().type == TokenType::Newline && depth_ > 0) {
            advance();
            continue;
        }
        if (at_end()) break;

        TokenType type = peek().type;
        if (type == TokenType::Dot || type == TokenType::LBracket || type == TokenType::LParen) {
            lhs = parse_postfix(std::move(lhs));
            continue;
        }

        int lbp = infix_binding_power(type);
        if (lbp < min_bp) break;

        Token op = advance();
        ExprPtr rhs = parse_expression(right_binding_power(op.type));
        lhs = make_binary(op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_prefix() {
    while (peek().type == TokenType::Newline && depth_ > 0) advance();

    if (at_end()) error(peek(), "unexpected end of input in expression");

    Token tok = advance();

    switch (tok.type) {
        case TokenType::Integer:
        case TokenType::Float:
        case TokenType::String: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->kind = tok.type == TokenType::Integer ? LiteralExpr::Kind::Integer
                    : tok.type == TokenType::Float ? LiteralExpr::Kind::Float
                    : LiteralExpr::Kind::String;
            e->value = tok.lexeme;
            return e;
        }
        case TokenType::True:
        case TokenType::False: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->kind = LiteralExpr::Kind::Bool;
            e->value = tok.lexeme;
            return e;
        }
        case TokenType::Nil: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->kind = LiteralExpr::Kind::Nil;
            return e;
        }
        case TokenType::Identifier: {
            auto e = std::make_unique<IdentExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->name = tok.lexeme;
            return e;
        }
        case TokenType::LParen: {
            ExprPtr inner = parse_expression(0);
            if (!match(TokenType::RParen)) error(peek(), "expected )");
            auto e = std::make_unique<GroupExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->expr = std::move(inner);
            return e;
        }
        case TokenType::Minus:
        case TokenType::Bang:
        case TokenType::Tilde:
            return parse_unary(tok);
        case TokenType::BangType:
        case TokenType::BangTildeType:
        case TokenType::BangBangType:
        case TokenType::QueryType:
            return parse_rbt(tok);
        default:
            error(tok, "unexpected token in expression");
    }
}

ExprPtr Parser::parse_unary(const Token& op) {
    ExprPtr operand = parse_expression(kUnaryBp);
    auto e = std::make_unique<UnaryExpr>();
    e->line = op.line;
    e->column = op.column;
    e->op = op.lexeme;
    e->operand = std::move(operand);
    return e;
}

ExprPtr Parser::parse_rbt(const Token& op) {
    auto e = std::make_unique<RbtExpr>();
    e->line = op.line;
    e->column = op.column;
    switch (op.type) {
        case TokenType::BangType:
            e->op = RbtExpr::Op::Prove;
            e->type_name = op.lexeme.substr(1);
            break;
        case TokenType::BangTildeType:
            e->op = RbtExpr::Op::Mask;
            e->type_name = op.lexeme.substr(2);
            break;
        case TokenType::BangBangType:
            e->op = RbtExpr::Op::DeepProve;
            e->type_name = op.lexeme.substr(2);
            break;
        case TokenType::QueryType:
            e->op = RbtExpr::Op::Query;
            e->type_name = op.lexeme.substr(1);
            break;
        default:
            break;
    }
    e->operand = parse_expression(kUnaryBp);
    return e;
}

ExprPtr Parser::parse_postfix(ExprPtr lhs) {
    Token tok = advance();
    switch (tok.type) {
        case TokenType::LParen: {
            auto e = std::make_unique<CallExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->callee = std::move(lhs);
            if (!check(TokenType::RParen)) {
                do {
                    e->args.push_back(parse_expression(0));
                } while (match(TokenType::Comma));
            }
            if (!match(TokenType::RParen)) error(peek(), "expected ) after arguments");
            return e;
        }
        case TokenType::LBracket: {
            auto e = std::make_unique<IndexExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->index = parse_expression(0);
            if (!match(TokenType::RBracket)) error(peek(), "expected ] after index");
            e->target = std::move(lhs);
            return e;
        }
        case TokenType::Dot: {
            Token name = advance();
            if (name.type != TokenType::Identifier) error(name, "expected member name after .");
            auto e = std::make_unique<MemberExpr>();
            e->line = tok.line;
            e->column = tok.column;
            e->name = name.lexeme;
            e->target = std::move(lhs);
            return e;
        }
        default:
            error(tok, "invalid postfix operator");
    }
}

ExprPtr Parser::make_binary(const Token& op, ExprPtr left, ExprPtr right) {
    auto e = std::make_unique<BinaryExpr>();
    e->line = op.line;
    e->column = op.column;
    e->op = op.lexeme;
    e->left = std::move(left);
    e->right = std::move(right);
    return e;
}

int Parser::infix_binding_power(TokenType type) const {
    switch (type) {
        case TokenType::Equal:
            return 1;
        case TokenType::PipePipe:
            return 2;
        case TokenType::AmpersandAmpersand:
            return 3;
        case TokenType::EqualEqual:
        case TokenType::BangEqual:
        case TokenType::Less:
        case TokenType::Greater:
        case TokenType::LessEqual:
        case TokenType::GreaterEqual:
            return 4;
        case TokenType::Plus:
        case TokenType::Minus:
            return 5;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
            return 6;
        default:
            return -1;
    }
}

int Parser::right_binding_power(TokenType type) const {
    int lbp = infix_binding_power(type);
    if (type == TokenType::Equal) return lbp;
    return lbp + 1;
}

std::string Parser::parse_type_name() {
    Token first = peek();
    if (!match(TokenType::Identifier)) error(first, "expected type name");

    std::string name = previous().lexeme;
    if (match(TokenType::Less)) {
        name += "<";
        while (true) {
            if (!match(TokenType::Identifier)) error(peek(), "expected type name");
            name += previous().lexeme;
            if (match(TokenType::Comma)) {
                name += ",";
                continue;
            }
            break;
        }
        if (!match(TokenType::Greater)) error(peek(), "expected >");
        name += ">";
    }
    return name;
}

StmtPtr Parser::parse_statement() {
    if (match(TokenType::Let)) return parse_let();
    if (check(TokenType::LBrace)) return parse_block();
    return parse_expression_statement();
}

StmtPtr Parser::parse_let() {
    Token name_tok = peek();
    if (!match(TokenType::Identifier)) error(name_tok, "expected variable name after let");

    auto stmt = std::make_unique<LetStmt>();
    stmt->line = name_tok.line;
    stmt->column = name_tok.column;
    stmt->name = name_tok.lexeme;

    if (match(TokenType::ColonColon)) {
        stmt->type_name = parse_type_name();
        stmt->has_type = true;
    }

    if (!match(TokenType::Equal)) error(peek(), "expected = in let");
    stmt->value = parse_expression(0);
    return stmt;
}

StmtPtr Parser::parse_block() {
    Token open = peek();
    if (!match(TokenType::LBrace)) error(open, "expected {");

    auto block = std::make_unique<BlockStmt>();
    block->line = open.line;
    block->column = open.column;

    skip_newlines();
    while (!check(TokenType::RBrace) && !at_end()) {
        block->stmts.push_back(parse_statement());
        skip_newlines();
    }
    if (!match(TokenType::RBrace)) error(peek(), "expected }");
    return block;
}

StmtPtr Parser::parse_expression_statement() {
    Token start = peek();
    ExprPtr expr = parse_expression(0);
    auto stmt = std::make_unique<ExprStmt>();
    stmt->line = start.line;
    stmt->column = start.column;
    stmt->expr = std::move(expr);
    return stmt;
}

} // namespace bang
