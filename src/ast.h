#pragma once

#include <memory>
#include <string>
#include <vector>

namespace bang {

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    size_t line = 0;
    size_t column = 0;
    virtual ~Expr() = default;
};

struct LiteralExpr : Expr {
    enum class Kind { Integer, Float, String, Bool, Nil };
    Kind kind = Kind::Nil;
    std::string value;
};

struct IdentExpr : Expr {
    std::string name;
};

struct GroupExpr : Expr {
    ExprPtr expr;
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr operand;
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
};

struct RbtExpr : Expr {
    enum class Op { Prove, Mask, DeepProve, Query };
    Op op = Op::Prove;
    std::string type_name;
    ExprPtr operand;
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct IndexExpr : Expr {
    ExprPtr target;
    ExprPtr index;
};

struct MemberExpr : Expr {
    ExprPtr target;
    std::string name;
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

struct Stmt {
    size_t line = 0;
    size_t column = 0;
    virtual ~Stmt() = default;
};

struct LetStmt : Stmt {
    std::string name;
    std::string type_name;
    bool has_type = false;
    ExprPtr value;
};

struct ExprStmt : Stmt {
    ExprPtr expr;
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
};

} // namespace bang
