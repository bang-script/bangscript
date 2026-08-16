#pragma once

#include "ast.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace bang {

// ============================================================================
// TYPE SYSTEM
// ============================================================================

class Type {
public:
    enum class Kind {
        Unknown,    // Top type — anything
        Nil,
        Bool,
        Integer,
        Float,
        String,
        List,
        Function,
        Error       // Sentinel for type errors (poison)
    };

    Kind kind;
    std::vector<std::shared_ptr<Type>> params;           // For List<T>, etc.
    std::vector<std::shared_ptr<Type>> param_types;      // For functions
    std::shared_ptr<Type> return_type;                   // For functions

    Type(Kind k) : kind(k) {}

    static std::shared_ptr<Type> make(Kind k);
    static std::shared_ptr<Type> make_list(std::shared_ptr<Type> elem);
    static std::shared_ptr<Type> make_function(
        std::vector<std::shared_ptr<Type>> params,
        std::shared_ptr<Type> ret
    );

    bool is_subtype_of(const std::shared_ptr<Type>& other) const;
    bool is_disjoint_from(const std::shared_ptr<Type>& other) const;
    bool equals(const std::shared_ptr<Type>& other) const;
    std::string to_string() const;
};

// ============================================================================
// SYMBOL TABLE
// ============================================================================

enum class SymbolKind {
    Variable,
    Function,
    Parameter,
    LoopVar
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    std::shared_ptr<Type> type;
    bool is_mutable = true;
    size_t line = 0;
    size_t column = 0;
    // For RBT flow analysis: known runtime type constraints
    std::optional<std::shared_ptr<Type>> known_type;
    // For functions: is TCO-enabled (ld)
    bool is_tco = false;
};

class Scope {
public:
    Scope* parent = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> bindings;

    explicit Scope(Scope* p = nullptr) : parent(p) {}

    void define(const std::string& name, std::shared_ptr<Symbol> sym);
    std::shared_ptr<Symbol> lookup(const std::string& name) const;
    std::shared_ptr<Symbol> lookup_local(const std::string& name) const;
};

// ============================================================================
// FLOW-SENSITIVE RBT: TYPE ASSUMPTIONS
// ============================================================================

// Represents what we know about a variable's type at a specific program point.
// Used for flow-sensitive RBT narrowing.
struct TypeAssumption {
    std::string var_name;
    std::shared_ptr<Type> assumed_type;
    bool is_positive;  // true = we know it IS this type, false = we know it ISN'T
};

// A set of assumptions active at a given point in the program.
using AssumptionSet = std::vector<TypeAssumption>;

// ============================================================================
// SEMANTIC ANALYZER
// ============================================================================

struct SemanticError {
    size_t line;
    size_t column;
    std::string message;
    bool is_warning = false;
};

enum class RbtAction {
    Elide,
    Check,
    CheckOrNil,
    Error
};

struct RbtResult {
    RbtAction action;
    std::shared_ptr<Type> narrowed_type;
    std::string reason;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    bool analyze(const std::vector<StmtPtr>& stmts);

    const std::vector<SemanticError>& errors() const { return errors_; }
    const std::vector<SemanticError>& warnings() const { return warnings_; }

private:
    std::vector<SemanticError> errors_;
    std::vector<SemanticError> warnings_;
    std::unique_ptr<Scope> current_scope_;
    std::shared_ptr<Type> current_return_type_;
    bool inside_loop_ = false;
    bool inside_function_ = false;

    // Flow-sensitive assumptions: what we know about variables at current point
    AssumptionSet current_assumptions_;

    // Scope management
    struct ScopeGuard {
        SemanticAnalyzer* analyzer;
        Scope* prev_scope;
        ScopeGuard(SemanticAnalyzer* a, Scope* parent);
        ~ScopeGuard();
    };

    void push_scope();
    void pop_scope();

    // Core analysis
    void analyze_stmt(const StmtPtr& stmt);
    void analyze_stmt(const StmtPtr& stmt, const AssumptionSet& assumptions);
    std::shared_ptr<Type> analyze_expr(const ExprPtr& expr);
    std::shared_ptr<Type> analyze_expr(const ExprPtr& expr, const AssumptionSet& assumptions);

    // Flow-sensitive helpers
    AssumptionSet extract_positive_assumptions(const ExprPtr& condition);
    AssumptionSet extract_negative_assumptions(const ExprPtr& condition);
    std::shared_ptr<Type> get_known_type(const std::string& name, const AssumptionSet& assumptions);
    AssumptionSet merge_assumptions(const AssumptionSet& a, const AssumptionSet& b);
    bool assumptions_imply_type(const std::string& name, const std::shared_ptr<Type>& type,
                                 const AssumptionSet& assumptions);

    // Statements
    void analyze_let(const LetStmt& stmt);
    void analyze_expr_stmt(const ExprStmt& stmt);
    void analyze_block(const BlockStmt& stmt);
    void analyze_block(const BlockStmt& stmt, const AssumptionSet& assumptions);
    void analyze_fn(const FnStmt& stmt);
    void analyze_ld(const LdStmt& stmt);
    void analyze_for(const ForStmt& stmt);
    void analyze_if(const IfStmt& stmt);
    void analyze_match(const MatchStmt& stmt);
    void analyze_import(const ImportStmt& stmt);
    void analyze_return(const ReturnStmt& stmt);
    void analyze_break(const BreakStmt& stmt);
    void analyze_continue(const ContinueStmt& stmt);

    // Expressions
    std::shared_ptr<Type> analyze_literal(const LiteralExpr& expr);
    std::shared_ptr<Type> analyze_ident(const IdentExpr& expr);
    std::shared_ptr<Type> analyze_ident(const IdentExpr& expr, const AssumptionSet& assumptions);
    std::shared_ptr<Type> analyze_unary(const UnaryExpr& expr);
    std::shared_ptr<Type> analyze_binary(const BinaryExpr& expr);
    std::shared_ptr<Type> analyze_call(const CallExpr& expr);
    std::shared_ptr<Type> analyze_index(const IndexExpr& expr);
    std::shared_ptr<Type> analyze_member(const MemberExpr& expr);
    std::shared_ptr<Type> analyze_group(const GroupExpr& expr);
    std::shared_ptr<Type> analyze_range(const RangeExpr& expr);
    std::shared_ptr<Type> analyze_lambda(const LambdaExpr& expr);
    std::shared_ptr<Type> analyze_match_expr(const MatchExpr& expr);
    std::shared_ptr<Type> analyze_rbt(const RbtExpr& expr);
    std::shared_ptr<Type> analyze_rbt(const RbtExpr& expr, const AssumptionSet& assumptions);

    // RBT
    RbtResult compute_rbt_action(
        const std::shared_ptr<Type>& operand_type,
        const std::shared_ptr<Type>& target_type,
        RbtExpr::Op op,
        const AssumptionSet& assumptions,
        const std::string& operand_name  // for flow-sensitive lookup
    );

    // Type helpers
    std::shared_ptr<Type> parse_type_string(const std::string& str);
    std::shared_ptr<Type> resolve_type(const std::string& name);
    std::shared_ptr<Type> resolve_param_type(const std::optional<std::string>& name);

    // Errors
    void error(size_t line, size_t col, const std::string& msg);
    void warning(size_t line, size_t col, const std::string& msg);
};

} // namespace bang
