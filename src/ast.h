#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace bang {

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    size_t line = 0;
    size_t column = 0;
    virtual ~Expr() = default;
};

// ============================================================================
// EXPRESSIONS
// ============================================================================

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

// Range expression: 1..=n or 1..n
struct RangeExpr : Expr {
    ExprPtr start;
    ExprPtr end;
    bool inclusive = false;  // true for ..=, false for ..
};

// Lambda expression: (params) -> { body }
struct LambdaExpr : Expr {
    struct Param {
        std::string name;
        std::optional<std::string> type_name;
    };
    std::vector<Param> params;
    std::optional<std::string> return_type_name;
    StmtPtr body;  // BlockStmt
};

// Match arm: pattern => expr
struct MatchArm {
    ExprPtr pattern;  // Literal, Ident (binding), or Wildcard (_)
    ExprPtr body;
};

struct MatchExpr : Expr {
    ExprPtr scrutinee;
    std::vector<MatchArm> arms;
};

// ============================================================================
// STATEMENTS
// ============================================================================

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

// fn name(params) :: ReturnType { body }
// or: fn name(params) { body }  (inferred return)
struct FnStmt : Stmt {
    std::string name;
    struct Param {
        std::string name;
        std::optional<std::string> type_name;
    };
    std::vector<Param> params;
    std::optional<std::string> return_type_name;
    StmtPtr body;  // BlockStmt
};

// ld name(params) -> { body }  (TCO lambda, always returns implicitly)
struct LdStmt : Stmt {
    std::string name;
    struct Param {
        std::string name;
        std::optional<std::string> type_name;
        std::optional<ExprPtr> default_value;
    };
    std::vector<Param> params;
    StmtPtr body;  // BlockStmt
};

// for binding in iterable { body }
struct ForStmt : Stmt {
    std::string binding;
    ExprPtr iterable;
    StmtPtr body;  // BlockStmt
};

// if cond { then_branch } else { else_branch }
// else if is parsed as else { if ... }
struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr then_branch;   // BlockStmt
    StmtPtr else_branch;   // BlockStmt or IfStmt (else if), or nullptr
};

// match scrutinee { arm => body, ... }
struct MatchStmt : Stmt {
    ExprPtr scrutinee;
    std::vector<MatchArm> arms;
};

// import { name1, name2 } from "module"
struct ImportStmt : Stmt {
    std::vector<std::string> names;
    std::string module_path;
};

// return expr?
struct ReturnStmt : Stmt {
    std::optional<ExprPtr> value;
};

// break
struct BreakStmt : Stmt {
};

// continue
struct ContinueStmt : Stmt {
};

} // namespace bang
