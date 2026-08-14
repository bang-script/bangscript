#pragma once

#include <string>
#include <vector>

#include "ast.h"
#include "lexer.h"

namespace bang {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::vector<StmtPtr> parse();

    const std::vector<std::string>& errors() const { return errors_; }

private:
    static const int kUnaryBp = 7;

    std::vector<Token> tokens_;
    size_t current_ = 0;
    int depth_ = 0;
    std::vector<std::string> errors_;

    const Token& peek() const;
    const Token& previous() const;
    bool at_end() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token advance();
    void skip_newlines();

    [[noreturn]] void error(const Token& tok, const std::string& msg);

    ExprPtr parse_expression(int min_bp);
    ExprPtr parse_prefix();
    ExprPtr parse_postfix(ExprPtr lhs);
    ExprPtr parse_unary(const Token& op);
    ExprPtr parse_rbt(const Token& op);
    ExprPtr make_binary(const Token& op, ExprPtr left, ExprPtr right);
    int infix_binding_power(TokenType type) const;
    int right_binding_power(TokenType type) const;

    std::string parse_type_name();

    StmtPtr parse_statement();
    StmtPtr parse_let();
    StmtPtr parse_block();
    StmtPtr parse_expression_statement();
};

} // namespace bang
