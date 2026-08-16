#include "semantic.h"

#include <sstream>
#include <cctype>
#include <algorithm>

namespace bang {

// ============================================================================
// TYPE SYSTEM
// ============================================================================

std::shared_ptr<Type> Type::make(Kind k) {
    static std::unordered_map<Kind, std::shared_ptr<Type>> primitives;
    if (primitives.empty()) {
        primitives[Kind::Unknown] = std::make_shared<Type>(Kind::Unknown);
        primitives[Kind::Nil]     = std::make_shared<Type>(Kind::Nil);
        primitives[Kind::Bool]    = std::make_shared<Type>(Kind::Bool);
        primitives[Kind::Integer] = std::make_shared<Type>(Kind::Integer);
        primitives[Kind::Float]   = std::make_shared<Type>(Kind::Float);
        primitives[Kind::String]  = std::make_shared<Type>(Kind::String);
        primitives[Kind::Error]   = std::make_shared<Type>(Kind::Error);
    }
    auto it = primitives.find(k);
    if (it != primitives.end()) return it->second;
    return std::make_shared<Type>(k);
}

std::shared_ptr<Type> Type::make_list(std::shared_ptr<Type> elem) {
    auto t = std::make_shared<Type>(Kind::List);
    t->params.push_back(std::move(elem));
    return t;
}

std::shared_ptr<Type> Type::make_function(
    std::vector<std::shared_ptr<Type>> params,
    std::shared_ptr<Type> ret
) {
    auto t = std::make_shared<Type>(Kind::Function);
    t->param_types = std::move(params);
    t->return_type = std::move(ret);
    return t;
}

bool Type::is_subtype_of(const std::shared_ptr<Type>& other) const {
    if (kind == Kind::Error || other->kind == Kind::Error) return true;
    if (kind == Kind::Unknown) return true;
    if (other->kind == Kind::Unknown) return true;
    if (kind != other->kind) return false;

    if (kind == Kind::List) {
        if (params.empty() || other->params.empty()) return true;
        return params[0]->is_subtype_of(other->params[0]);
    }
    if (kind == Kind::Function) {
        if (param_types.size() != other->param_types.size()) return false;
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (!param_types[i]->is_subtype_of(other->param_types[i])) return false;
        }
        return return_type->is_subtype_of(other->return_type);
    }
    return true;
}

bool Type::is_disjoint_from(const std::shared_ptr<Type>& other) const {
    if (kind == Kind::Error || other->kind == Kind::Error) return false;
    if (kind == Kind::Unknown || other->kind == Kind::Unknown) return false;
    if (kind == Kind::Nil && other->kind != Kind::Nil) return true;
    if (kind != Kind::Nil && other->kind == Kind::Nil) return true;
    if (kind != other->kind) return true;

    if (kind == Kind::List) {
        if (params.empty() || other->params.empty()) return false;
        return params[0]->is_disjoint_from(other->params[0]);
    }
    return false;
}

bool Type::equals(const std::shared_ptr<Type>& other) const {
    if (!other) return false;
    if (kind != other->kind) return false;
    if (kind == Kind::List) {
        if (params.size() != other->params.size()) return false;
        for (size_t i = 0; i < params.size(); ++i) {
            if (!params[i]->equals(other->params[i])) return false;
        }
        return true;
    }
    if (kind == Kind::Function) {
        if (param_types.size() != other->param_types.size()) return false;
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (!param_types[i]->equals(other->param_types[i])) return false;
        }
        if (!return_type->equals(other->return_type)) return false;
        return true;
    }
    return true;
}

std::string Type::to_string() const {
    switch (kind) {
        case Kind::Unknown: return "Unknown";
        case Kind::Nil:     return "Nil";
        case Kind::Bool:    return "Bool";
        case Kind::Integer: return "Integer";
        case Kind::Float:   return "Float";
        case Kind::String:  return "String";
        case Kind::List: {
            if (params.empty()) return "List";
            return "List<" + params[0]->to_string() + ">";
        }
        case Kind::Function: {
            std::string s = "fn(";
            for (size_t i = 0; i < param_types.size(); ++i) {
                if (i > 0) s += ", ";
                s += param_types[i]->to_string();
            }
            s += ") -> " + return_type->to_string();
            return s;
        }
        case Kind::Error:   return "<error>";
    }
    return "<?>";
}

// ============================================================================
// SCOPE
// ============================================================================

void Scope::define(const std::string& name, std::shared_ptr<Symbol> sym) {
    bindings[name] = std::move(sym);
}

std::shared_ptr<Symbol> Scope::lookup(const std::string& name) const {
    auto it = bindings.find(name);
    if (it != bindings.end()) return it->second;
    if (parent) return parent->lookup(name);
    return nullptr;
}

std::shared_ptr<Symbol> Scope::lookup_local(const std::string& name) const {
    auto it = bindings.find(name);
    if (it != bindings.end()) return it->second;
    return nullptr;
}

// ============================================================================
// SCOPE GUARD
// ============================================================================

SemanticAnalyzer::ScopeGuard::ScopeGuard(SemanticAnalyzer* a, Scope* parent)
    : analyzer(a) {
    analyzer->current_scope_ = std::make_unique<Scope>(parent);
}

SemanticAnalyzer::ScopeGuard::~ScopeGuard() {
    analyzer->current_scope_ = std::unique_ptr<Scope>(analyzer->current_scope_->parent);
}

void SemanticAnalyzer::push_scope() {
    current_scope_ = std::make_unique<Scope>(current_scope_.release());
}

void SemanticAnalyzer::pop_scope() {
    current_scope_ = std::unique_ptr<Scope>(current_scope_->parent);
}

// ============================================================================
// SEMANTIC ANALYZER
// ============================================================================

SemanticAnalyzer::SemanticAnalyzer()
    : current_scope_(std::make_unique<Scope>(nullptr)),
      current_return_type_(Type::make(Type::Kind::Unknown)) {}

bool SemanticAnalyzer::analyze(const std::vector<StmtPtr>& stmts) {
    errors_.clear();
    warnings_.clear();
    current_scope_ = std::make_unique<Scope>(nullptr);
    current_return_type_ = Type::make(Type::Kind::Unknown);
    inside_loop_ = false;
    inside_function_ = false;

    // Predefine built-in functions
    auto output_sym = std::make_shared<Symbol>();
    output_sym->name = "output";
    output_sym->kind = SymbolKind::Function;
    output_sym->type = Type::make_function(
        {Type::make(Type::Kind::Unknown)},
        Type::make(Type::Kind::Nil)
    );
    current_scope_->define("output", output_sym);

    for (const auto& stmt : stmts) {
        analyze_stmt(stmt);
    }

    return errors_.empty();
}

// ============================================================================
// STATEMENT DISPATCH
// ============================================================================

void SemanticAnalyzer::analyze_stmt(const StmtPtr& stmt) {
    if (auto let = dynamic_cast<LetStmt*>(stmt.get())) {
        analyze_let(*let);
    } else if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        analyze_expr_stmt(*expr_stmt);
    } else if (auto block = dynamic_cast<BlockStmt*>(stmt.get())) {
        analyze_block(*block);
    } else if (auto fn = dynamic_cast<FnStmt*>(stmt.get())) {
        analyze_fn(*fn);
    } else if (auto ld = dynamic_cast<LdStmt*>(stmt.get())) {
        analyze_ld(*ld);
    } else if (auto for_stmt = dynamic_cast<ForStmt*>(stmt.get())) {
        analyze_for(*for_stmt);
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        analyze_if(*if_stmt);
    } else if (auto match = dynamic_cast<MatchStmt*>(stmt.get())) {
        analyze_match(*match);
    } else if (auto import = dynamic_cast<ImportStmt*>(stmt.get())) {
        analyze_import(*import);
    } else if (auto ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
        analyze_return(*ret);
    } else if (auto brk = dynamic_cast<BreakStmt*>(stmt.get())) {
        analyze_break(*brk);
    } else if (auto cont = dynamic_cast<ContinueStmt*>(stmt.get())) {
        analyze_continue(*cont);
    }
}

// ============================================================================
// LET
// ============================================================================

void SemanticAnalyzer::analyze_let(const LetStmt& stmt) {
    auto value_type = analyze_expr(stmt.value);

    std::shared_ptr<Type> declared_type;
    if (stmt.has_type) {
        declared_type = parse_type_string(stmt.type_name);
        if (!value_type->is_subtype_of(declared_type)) {
            error(stmt.line, stmt.column,
                "cannot assign value of type \"" + value_type->to_string() +
                "\" to variable \"" + stmt.name + "\" of type \"" +
                declared_type->to_string() + "\"");
        }
    } else {
        declared_type = value_type;
    }

    auto sym = std::make_shared<Symbol>();
    sym->name = stmt.name;
    sym->kind = SymbolKind::Variable;
    sym->type = declared_type;
    sym->line = stmt.line;
    sym->column = stmt.column;

    // Known type for literals
    if (auto lit = dynamic_cast<LiteralExpr*>(stmt.value.get())) {
        switch (lit->kind) {
            case LiteralExpr::Kind::Integer: sym->known_type = Type::make(Type::Kind::Integer); break;
            case LiteralExpr::Kind::Float:   sym->known_type = Type::make(Type::Kind::Float); break;
            case LiteralExpr::Kind::String:  sym->known_type = Type::make(Type::Kind::String); break;
            case LiteralExpr::Kind::Bool:    sym->known_type = Type::make(Type::Kind::Bool); break;
            case LiteralExpr::Kind::Nil:     sym->known_type = Type::make(Type::Kind::Nil); break;
        }
    }

    current_scope_->define(stmt.name, sym);
}

// ============================================================================
// EXPR STMT / BLOCK
// ============================================================================

void SemanticAnalyzer::analyze_expr_stmt(const ExprStmt& stmt) {
    analyze_expr(stmt.expr);
}

void SemanticAnalyzer::analyze_block(const BlockStmt& stmt) {
    ScopeGuard guard(this, current_scope_.get());
    for (const auto& s : stmt.stmts) {
        analyze_stmt(s);
    }
}

// ============================================================================
// FN (regular function)
// ============================================================================

void SemanticAnalyzer::analyze_fn(const FnStmt& stmt) {
    // Build function type from signature
    std::vector<std::shared_ptr<Type>> param_types;
    for (const auto& p : stmt.params) {
        param_types.push_back(resolve_param_type(p.type_name));
    }

    std::shared_ptr<Type> ret_type;
    if (stmt.return_type_name) {
        ret_type = parse_type_string(*stmt.return_type_name);
    } else {
        ret_type = Type::make(Type::Kind::Unknown);
    }

    auto fn_type = Type::make_function(std::move(param_types), ret_type);

    // Define the function symbol in current scope BEFORE analyzing body
    // (allows recursion)
    auto fn_sym = std::make_shared<Symbol>();
    fn_sym->name = stmt.name;
    fn_sym->kind = SymbolKind::Function;
    fn_sym->type = fn_type;
    fn_sym->line = stmt.line;
    fn_sym->column = stmt.column;
    fn_sym->is_tco = false;
    current_scope_->define(stmt.name, fn_sym);

    // Analyze body in new scope with parameters
    ScopeGuard guard(this, current_scope_.get());

    auto saved_return = current_return_type_;
    auto saved_in_fn = inside_function_;
    current_return_type_ = ret_type;
    inside_function_ = true;

    // Define parameters
    for (size_t i = 0; i < stmt.params.size(); ++i) {
        auto p = std::make_shared<Symbol>();
        p->name = stmt.params[i].name;
        p->kind = SymbolKind::Parameter;
        p->type = fn_type->param_types[i];
        p->line = stmt.line;
        p->column = stmt.column;
        current_scope_->define(stmt.params[i].name, p);
    }

    analyze_stmt(stmt.body);

    current_return_type_ = saved_return;
    inside_function_ = saved_in_fn;
}

// ============================================================================
// LD (TCO lambda)
// ============================================================================

void SemanticAnalyzer::analyze_ld(const LdStmt& stmt) {
    // ld infers return type from body — we do a two-pass approach:
    // First pass: define the function symbol with Unknown return
    // Second pass: analyze body, then update return type

    std::vector<std::shared_ptr<Type>> param_types;
    for (const auto& p : stmt.params) {
        param_types.push_back(resolve_param_type(p.type_name));
    }

    auto fn_type = Type::make_function(param_types, Type::make(Type::Kind::Unknown));

    auto fn_sym = std::make_shared<Symbol>();
    fn_sym->name = stmt.name;
    fn_sym->kind = SymbolKind::Function;
    fn_sym->type = fn_type;
    fn_sym->line = stmt.line;
    fn_sym->column = stmt.column;
    fn_sym->is_tco = true;
    current_scope_->define(stmt.name, fn_sym);

    // Analyze body
    ScopeGuard guard(this, current_scope_.get());

    auto saved_return = current_return_type_;
    auto saved_in_fn = inside_function_;
    current_return_type_ = Type::make(Type::Kind::Unknown);
    inside_function_ = true;

    for (size_t i = 0; i < stmt.params.size(); ++i) {
        auto p = std::make_shared<Symbol>();
        p->name = stmt.params[i].name;
        p->kind = SymbolKind::Parameter;
        p->type = fn_type->param_types[i];

        // Handle default values
        if (stmt.params[i].default_value) {
            auto default_type = analyze_expr(*stmt.params[i].default_value);
            if (!default_type->is_subtype_of(p->type)) {
                error(stmt.line, stmt.column,
                    "default value for parameter \"" + stmt.params[i].name +
                    "\" has type \"" + default_type->to_string() +
                    "\", expected \"" + p->type->to_string() + "\"");
            }
        }

        current_scope_->define(stmt.params[i].name, p);
    }

    analyze_stmt(stmt.body);

    // Update the function's return type based on what we inferred
    fn_type->return_type = current_return_type_;

    current_return_type_ = saved_return;
    inside_function_ = saved_in_fn;
}

// ============================================================================
// FOR
// ============================================================================

void SemanticAnalyzer::analyze_for(const ForStmt& stmt) {
    auto iterable_type = analyze_expr(stmt.iterable);

    // Determine element type
    std::shared_ptr<Type> elem_type;
    if (iterable_type->kind == Type::Kind::List) {
        if (!iterable_type->params.empty()) {
            elem_type = iterable_type->params[0];
        } else {
            elem_type = Type::make(Type::Kind::Unknown);
        }
    } else if (iterable_type->kind == Type::Kind::String) {
        // String iteration yields String (characters)
        elem_type = Type::make(Type::Kind::String);
    } else if (iterable_type->kind == Type::Kind::Unknown) {
        elem_type = Type::make(Type::Kind::Unknown);
    } else {
        error(stmt.line, stmt.column,
            "cannot iterate over type \"" + iterable_type->to_string() + "\"");
        elem_type = Type::make(Type::Kind::Error);
    }

    ScopeGuard guard(this, current_scope_.get());

    auto saved_in_loop = inside_loop_;
    inside_loop_ = true;

    auto binding = std::make_shared<Symbol>();
    binding->name = stmt.binding;
    binding->kind = SymbolKind::LoopVar;
    binding->type = elem_type;
    binding->is_mutable = false;  // loop vars are immutable
    current_scope_->define(stmt.binding, binding);

    analyze_stmt(stmt.body);

    inside_loop_ = saved_in_loop;
}

// ============================================================================
// IF
// ============================================================================

void SemanticAnalyzer::analyze_if(const IfStmt& stmt) {
    auto cond_type = analyze_expr(stmt.condition);
    if (cond_type->kind != Type::Kind::Bool && cond_type->kind != Type::Kind::Unknown) {
        error(stmt.line, stmt.column,
            "if condition must be Bool, got \"" + cond_type->to_string() + "\"");
    }

    analyze_stmt(stmt.then_branch);

    if (stmt.else_branch) {
        analyze_stmt(stmt.else_branch);
    }
}

// ============================================================================
// MATCH
// ============================================================================

void SemanticAnalyzer::analyze_match(const MatchStmt& stmt) {
    auto scrutinee_type = analyze_expr(stmt.scrutinee);

    bool has_wildcard = false;

    for (const auto& arm : stmt.arms) {
        // Analyze pattern
        std::shared_ptr<Type> pattern_type;
        if (auto lit = dynamic_cast<LiteralExpr*>(arm.pattern.get())) {
            pattern_type = analyze_literal(*lit);
        } else if (auto ident = dynamic_cast<IdentExpr*>(arm.pattern.get())) {
            if (ident->name == "_") {
                has_wildcard = true;
                pattern_type = Type::make(Type::Kind::Unknown);
            } else {
                // Binding pattern — binds the scrutinee's type
                pattern_type = scrutinee_type;
                ScopeGuard guard(this, current_scope_.get());
                auto sym = std::make_shared<Symbol>();
                sym->name = ident->name;
                sym->kind = SymbolKind::Variable;
                sym->type = scrutinee_type;
                current_scope_->define(ident->name, sym);
                analyze_expr(arm.body);
                continue;  // body already analyzed
            }
        } else {
            error(arm.pattern->line, arm.pattern->column,
                "invalid match pattern");
            pattern_type = Type::make(Type::Kind::Error);
        }

        // Check pattern compatibility
        if (!pattern_type->is_subtype_of(scrutinee_type) &&
            !scrutinee_type->is_subtype_of(pattern_type)) {
            warning(arm.pattern->line, arm.pattern->column,
                "match pattern type \"" + pattern_type->to_string() +
                "\" may not match scrutinee type \"" + scrutinee_type->to_string() + "\"");
        }

        analyze_expr(arm.body);
    }

    if (!has_wildcard) {
        warning(stmt.line, stmt.column,
            "match expression without wildcard (_) pattern is not exhaustive");
    }
}

// ============================================================================
// IMPORT / RETURN / BREAK / CONTINUE
// ============================================================================

void SemanticAnalyzer::analyze_import(const ImportStmt& stmt) {
    // For now, just validate that imported names don't conflict
    for (const auto& name : stmt.names) {
        if (current_scope_->lookup_local(name)) {
            error(stmt.line, stmt.column,
                "imported name \"" + name + "\" conflicts with existing binding");
        }
        // Mark as Unknown for now — resolved at link/load time
        auto sym = std::make_shared<Symbol>();
        sym->name = name;
        sym->kind = SymbolKind::Variable;
        sym->type = Type::make(Type::Kind::Unknown);
        current_scope_->define(name, sym);
    }
}

void SemanticAnalyzer::analyze_return(const ReturnStmt& stmt) {
    if (!inside_function_) {
        error(stmt.line, stmt.column,
            "return outside of function");
        return;
    }

    std::shared_ptr<Type> ret_type;
    if (stmt.value) {
        ret_type = analyze_expr(*stmt.value);
    } else {
        ret_type = Type::make(Type::Kind::Nil);
    }

    // Update inferred return type (for ld functions)
    if (current_return_type_->kind == Type::Kind::Unknown) {
        current_return_type_ = ret_type;
    } else if (!ret_type->is_subtype_of(current_return_type_)) {
        error(stmt.line, stmt.column,
            "return type \"" + ret_type->to_string() +
            "\" does not match function return type \"" +
            current_return_type_->to_string() + "\"");
    }
}

void SemanticAnalyzer::analyze_break(const BreakStmt& stmt) {
    if (!inside_loop_) {
        error(stmt.line, stmt.column,
            "break outside of loop");
    }
}

void SemanticAnalyzer::analyze_continue(const ContinueStmt& stmt) {
    if (!inside_loop_) {
        error(stmt.line, stmt.column,
            "continue outside of loop");
    }
}

// ============================================================================
// EXPRESSION DISPATCH
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_expr(const ExprPtr& expr) {
    if (auto lit = dynamic_cast<LiteralExpr*>(expr.get())) {
        return analyze_literal(*lit);
    }
    if (auto ident = dynamic_cast<IdentExpr*>(expr.get())) {
        return analyze_ident(*ident);
    }
    if (auto unary = dynamic_cast<UnaryExpr*>(expr.get())) {
        return analyze_unary(*unary);
    }
    if (auto binary = dynamic_cast<BinaryExpr*>(expr.get())) {
        return analyze_binary(*binary);
    }
    if (auto call = dynamic_cast<CallExpr*>(expr.get())) {
        return analyze_call(*call);
    }
    if (auto index = dynamic_cast<IndexExpr*>(expr.get())) {
        return analyze_index(*index);
    }
    if (auto member = dynamic_cast<MemberExpr*>(expr.get())) {
        return analyze_member(*member);
    }
    if (auto group = dynamic_cast<GroupExpr*>(expr.get())) {
        return analyze_group(*group);
    }
    if (auto range = dynamic_cast<RangeExpr*>(expr.get())) {
        return analyze_range(*range);
    }
    if (auto lambda = dynamic_cast<LambdaExpr*>(expr.get())) {
        return analyze_lambda(*lambda);
    }
    if (auto match = dynamic_cast<MatchExpr*>(expr.get())) {
        return analyze_match_expr(*match);
    }
    if (auto rbt = dynamic_cast<RbtExpr*>(expr.get())) {
        return analyze_rbt(*rbt);
    }

    error(expr->line, expr->column, "unknown expression type in semantic analyzer");
    return Type::make(Type::Kind::Error);
}

// ============================================================================
// LITERAL / IDENT / GROUP
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_literal(const LiteralExpr& expr) {
    switch (expr.kind) {
        case LiteralExpr::Kind::Integer: return Type::make(Type::Kind::Integer);
        case LiteralExpr::Kind::Float:   return Type::make(Type::Kind::Float);
        case LiteralExpr::Kind::String:  return Type::make(Type::Kind::String);
        case LiteralExpr::Kind::Bool:    return Type::make(Type::Kind::Bool);
        case LiteralExpr::Kind::Nil:     return Type::make(Type::Kind::Nil);
    }
    return Type::make(Type::Kind::Error);
}

std::shared_ptr<Type> SemanticAnalyzer::analyze_ident(const IdentExpr& expr) {
    auto sym = current_scope_->lookup(expr.name);
    if (!sym) {
        error(expr.line, expr.column,
            "undefined variable \"" + expr.name + "\"");
        return Type::make(Type::Kind::Error);
    }
    return sym->type;
}

std::shared_ptr<Type> SemanticAnalyzer::analyze_group(const GroupExpr& expr) {
    return analyze_expr(expr.expr);
}

// ============================================================================
// UNARY
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_unary(const UnaryExpr& expr) {
    auto operand_type = analyze_expr(expr.operand);

    if (expr.op == "-") {
        if (operand_type->kind != Type::Kind::Integer &&
            operand_type->kind != Type::Kind::Float) {
            error(expr.line, expr.column,
                "unary '-' requires numeric operand, got \"" +
                operand_type->to_string() + "\"");
            return Type::make(Type::Kind::Error);
        }
        return operand_type;
    }

    if (expr.op == "!" || expr.op == "~") {
        if (operand_type->kind != Type::Kind::Bool) {
            error(expr.line, expr.column,
                "unary '" + expr.op + "' requires Bool operand, got \"" +
                operand_type->to_string() + "\"");
            return Type::make(Type::Kind::Error);
        }
        return Type::make(Type::Kind::Bool);
    }

    error(expr.line, expr.column,
        "unknown unary operator \"" + expr.op + "\"");
    return Type::make(Type::Kind::Error);
}

// ============================================================================
// BINARY
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_binary(const BinaryExpr& expr) {
    auto left_type = analyze_expr(expr.left);
    auto right_type = analyze_expr(expr.right);

    if (expr.op == "=") {
        if (!right_type->is_subtype_of(left_type)) {
            error(expr.line, expr.column,
                "cannot assign \"" + right_type->to_string() +
                "\" to \"" + left_type->to_string() + "\"");
        }
        return left_type;
    }

    if (expr.op == "==" || expr.op == "!=") {
        return Type::make(Type::Kind::Bool);
    }

    if (expr.op == "<" || expr.op == ">" || expr.op == "<=" || expr.op == ">=") {
        if ((left_type->kind != Type::Kind::Integer && left_type->kind != Type::Kind::Float) ||
            (right_type->kind != Type::Kind::Integer && right_type->kind != Type::Kind::Float)) {
            error(expr.line, expr.column,
                "comparison operators require numeric operands");
            return Type::make(Type::Kind::Error);
        }
        return Type::make(Type::Kind::Bool);
    }

    if (expr.op == "||" || expr.op == "&&") {
        if (left_type->kind != Type::Kind::Bool || right_type->kind != Type::Kind::Bool) {
            error(expr.line, expr.column,
                "logical operators require Bool operands");
            return Type::make(Type::Kind::Error);
        }
        return Type::make(Type::Kind::Bool);
    }

    if (expr.op == "+" || expr.op == "-" || expr.op == "*" || expr.op == "/" || expr.op == "%") {
        if (expr.op == "+" && left_type->kind == Type::Kind::String && right_type->kind == Type::Kind::String) {
            return Type::make(Type::Kind::String);
        }

        if ((left_type->kind != Type::Kind::Integer && left_type->kind != Type::Kind::Float) ||
            (right_type->kind != Type::Kind::Integer && right_type->kind != Type::Kind::Float)) {
            error(expr.line, expr.column,
                "arithmetic operator \"" + expr.op +
                "\" requires numeric operands, got \"" +
                left_type->to_string() + "\" and \"" +
                right_type->to_string() + "\"");
            return Type::make(Type::Kind::Error);
        }

        if (left_type->kind == Type::Kind::Float || right_type->kind == Type::Kind::Float) {
            return Type::make(Type::Kind::Float);
        }
        return Type::make(Type::Kind::Integer);
    }

    error(expr.line, expr.column,
        "unknown binary operator \"" + expr.op + "\"");
    return Type::make(Type::Kind::Error);
}

// ============================================================================
// CALL
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_call(const CallExpr& expr) {
    auto callee_type = analyze_expr(expr.callee);

    if (callee_type->kind != Type::Kind::Function) {
        error(expr.line, expr.column,
            "cannot call non-function type \"" + callee_type->to_string() + "\"");
        return Type::make(Type::Kind::Error);
    }

    if (expr.args.size() != callee_type->param_types.size()) {
        error(expr.line, expr.column,
            "expected " + std::to_string(callee_type->param_types.size()) +
            " arguments, got " + std::to_string(expr.args.size()));
        return callee_type->return_type;
    }

    for (size_t i = 0; i < expr.args.size(); ++i) {
        auto arg_type = analyze_expr(expr.args[i]);
        if (!arg_type->is_subtype_of(callee_type->param_types[i])) {
            error(expr.line, expr.column,
                "argument " + std::to_string(i + 1) +
                " expects \"" + callee_type->param_types[i]->to_string() +
                "\", got \"" + arg_type->to_string() + "\"");
        }
    }

    return callee_type->return_type;
}

// ============================================================================
// INDEX
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_index(const IndexExpr& expr) {
    auto target_type = analyze_expr(expr.target);
    auto index_type = analyze_expr(expr.index);

    if (target_type->kind != Type::Kind::List) {
        error(expr.line, expr.column,
            "cannot index non-list type \"" + target_type->to_string() + "\"");
        return Type::make(Type::Kind::Error);
    }

    if (index_type->kind != Type::Kind::Integer) {
        error(expr.line, expr.column,
            "index must be Integer, got \"" + index_type->to_string() + "\"");
    }

    if (!target_type->params.empty()) {
        return target_type->params[0];
    }
    return Type::make(Type::Kind::Unknown);
}

// ============================================================================
// MEMBER
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_member(const MemberExpr& expr) {
    auto target_type = analyze_expr(expr.target);
    error(expr.line, expr.column,
        "member access not yet supported for type \"" + target_type->to_string() + "\"");
    return Type::make(Type::Kind::Error);
}

// ============================================================================
// RANGE
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_range(const RangeExpr& expr) {
    auto start_type = analyze_expr(expr.start);
    auto end_type = analyze_expr(expr.end);

    if (start_type->kind != Type::Kind::Integer || end_type->kind != Type::Kind::Integer) {
        error(expr.line, expr.column,
            "range bounds must be Integer");
        return Type::make(Type::Kind::Error);
    }

    // Range expression yields List<Integer>
    return Type::make_list(Type::make(Type::Kind::Integer));
}

// ============================================================================
// LAMBDA
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_lambda(const LambdaExpr& expr) {
    std::vector<std::shared_ptr<Type>> param_types;
    for (const auto& p : expr.params) {
        param_types.push_back(resolve_param_type(p.type_name));
    }

    std::shared_ptr<Type> ret_type;
    if (expr.return_type_name) {
        ret_type = parse_type_string(*expr.return_type_name);
    } else {
        ret_type = Type::make(Type::Kind::Unknown);
    }

    auto fn_type = Type::make_function(std::move(param_types), ret_type);

    // Analyze body in new scope
    ScopeGuard guard(this, current_scope_.get());

    auto saved_return = current_return_type_;
    auto saved_in_fn = inside_function_;
    current_return_type_ = ret_type;
    inside_function_ = true;

    for (size_t i = 0; i < expr.params.size(); ++i) {
        auto p = std::make_shared<Symbol>();
        p->name = expr.params[i].name;
        p->kind = SymbolKind::Parameter;
        p->type = fn_type->param_types[i];
        current_scope_->define(expr.params[i].name, p);
    }

    analyze_stmt(expr.body);

    // Update return type if inferred
    if (ret_type->kind == Type::Kind::Unknown) {
        fn_type->return_type = current_return_type_;
    }

    current_return_type_ = saved_return;
    inside_function_ = saved_in_fn;

    return fn_type;
}

// ============================================================================
// MATCH EXPR
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_match_expr(const MatchExpr& expr) {
    auto scrutinee_type = analyze_expr(expr.scrutinee);

    std::shared_ptr<Type> result_type = Type::make(Type::Kind::Unknown);
    bool has_wildcard = false;

    for (const auto& arm : expr.arms) {
        std::shared_ptr<Type> pattern_type;

        if (auto lit = dynamic_cast<LiteralExpr*>(arm.pattern.get())) {
            pattern_type = analyze_literal(*lit);
        } else if (auto ident = dynamic_cast<IdentExpr*>(arm.pattern.get())) {
            if (ident->name == "_") {
                has_wildcard = true;
                pattern_type = Type::make(Type::Kind::Unknown);
            } else {
                pattern_type = scrutinee_type;
                ScopeGuard guard(this, current_scope_.get());
                auto sym = std::make_shared<Symbol>();
                sym->name = ident->name;
                sym->kind = SymbolKind::Variable;
                sym->type = scrutinee_type;
                current_scope_->define(ident->name, sym);
                auto body_type = analyze_expr(arm.body);
                if (result_type->kind == Type::Kind::Unknown) {
                    result_type = body_type;
                }
                continue;
            }
        } else {
            error(arm.pattern->line, arm.pattern->column, "invalid match pattern");
            pattern_type = Type::make(Type::Kind::Error);
        }

        auto body_type = analyze_expr(arm.body);
        if (result_type->kind == Type::Kind::Unknown) {
            result_type = body_type;
        }
    }

    if (!has_wildcard) {
        warning(expr.line, expr.column,
            "match expression without wildcard (_) is not exhaustive");
    }

    return result_type;
}

// ============================================================================
// RBT
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::analyze_rbt(const RbtExpr& expr) {
    auto operand_type = analyze_expr(expr.operand);
    auto target_type = resolve_type(expr.type_name);

    auto result = compute_rbt_action(operand_type, target_type, expr.op);

    switch (result.action) {
        case RbtAction::Elide:
            return result.narrowed_type;
        case RbtAction::Check:
            return result.narrowed_type;
        case RbtAction::CheckOrNil:
            return Type::make(Type::Kind::Unknown);
        case RbtAction::Error:
            error(expr.line, expr.column, result.reason);
            return Type::make(Type::Kind::Error);
    }

    return Type::make(Type::Kind::Error);
}

RbtResult SemanticAnalyzer::compute_rbt_action(
    const std::shared_ptr<Type>& operand_type,
    const std::shared_ptr<Type>& target_type,
    RbtExpr::Op op
) {
    RbtResult result;
    result.narrowed_type = target_type;

    if (operand_type->kind == Type::Kind::Error) {
        result.action = RbtAction::Elide;
        result.reason = "operand has error type";
        return result;
    }

    if (operand_type->is_subtype_of(target_type)) {
        result.action = RbtAction::Elide;
        result.reason = "compile-time proved";
        return result;
    }

    if (operand_type->is_disjoint_from(target_type)) {
        if (op == RbtExpr::Op::Mask || op == RbtExpr::Op::Query) {
            result.action = RbtAction::CheckOrNil;
            result.reason = "disjoint types, masked operator allows fallback";
        } else {
            result.action = RbtAction::Error;
            result.reason = "Cannot prove that value is of type \"" +
                           target_type->to_string() +
                           "\" — type \"" + operand_type->to_string() +
                           "\" is disjoint from \"" + target_type->to_string() + "\"";
        }
        return result;
    }

    if (op == RbtExpr::Op::Mask) {
        result.action = RbtAction::CheckOrNil;
        result.reason = "runtime check with fallback to nil";
    } else if (op == RbtExpr::Op::Query) {
        result.action = RbtAction::CheckOrNil;
        result.reason = "runtime type query";
    } else {
        result.action = RbtAction::Check;
        result.reason = "runtime check required";
    }

    return result;
}

// ============================================================================
// TYPE PARSING
// ============================================================================

std::shared_ptr<Type> SemanticAnalyzer::parse_type_string(const std::string& str) {
    return resolve_type(str);
}

std::shared_ptr<Type> SemanticAnalyzer::resolve_type(const std::string& name) {
    if (name == "Unknown")  return Type::make(Type::Kind::Unknown);
    if (name == "Nil")      return Type::make(Type::Kind::Nil);
    if (name == "Bool")     return Type::make(Type::Kind::Bool);
    if (name == "Integer")  return Type::make(Type::Kind::Integer);
    if (name == "Float")    return Type::make(Type::Kind::Float);
    if (name == "String")   return Type::make(Type::Kind::String);

    auto lt_pos = name.find('<');
    if (lt_pos != std::string::npos) {
        auto base = name.substr(0, lt_pos);
        auto gt_pos = name.rfind('>');
        if (gt_pos == std::string::npos || gt_pos <= lt_pos) {
            return Type::make(Type::Kind::Error);
        }
        auto inner = name.substr(lt_pos + 1, gt_pos - lt_pos - 1);

        if (base == "List") {
            auto elem_type = resolve_type(inner);
            return Type::make_list(elem_type);
        }
    }

    return Type::make(Type::Kind::Unknown);
}

std::shared_ptr<Type> SemanticAnalyzer::resolve_param_type(const std::optional<std::string>& name) {
    if (name) return parse_type_string(*name);
    return Type::make(Type::Kind::Unknown);
}

// ============================================================================
// ERRORS
// ============================================================================

void SemanticAnalyzer::error(size_t line, size_t col, const std::string& msg) {
    errors_.push_back({line, col, msg, false});
}

void SemanticAnalyzer::warning(size_t line, size_t col, const std::string& msg) {
    warnings_.push_back({line, col, msg, true});
}

} // namespace bang
