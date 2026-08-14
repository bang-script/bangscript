#include "lexer.h"
#include "parser.h"
#include "ast.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdexcept>

using namespace bang;

// ---------------------------------------------------------------------------
// Simple test framework (no external dependencies)
// ---------------------------------------------------------------------------
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static std::string g_current_test;

#define TEST(name)     void test_##name();     struct test_reg_##name {         test_reg_##name() { register_test(#name, test_##name); }     } g_test_reg_##name;     void test_##name()

struct TestFunc {
    const char* name;
    void (*func)();
};

static std::vector<TestFunc>& test_registry() {
    static std::vector<TestFunc> reg;
    return reg;
}

static void register_test(const char* name, void (*func)()) {
    test_registry().push_back({name, func});
}

#define ASSERT(cond)     do {         g_tests_run++;         if (!(cond)) {             std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__                       << " Assertion failed: " << #cond << std::endl;             g_tests_failed++;             throw std::runtime_error("assertion failed");         } else {             g_tests_passed++;         }     } while (0)

#define ASSERT_EQ(a, b)     do {         g_tests_run++;         if ((a) != (b)) {             std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__                       << " Expected equality of:" << std::endl                       << "    " << #a << " (which is " << (a) << ")" << std::endl                       << "  and " << #b << " (which is " << (b) << ")" << std::endl;             g_tests_failed++;             throw std::runtime_error("assertion failed");         } else {             g_tests_passed++;         }     } while (0)

#define ASSERT_NE(a, b)     do {         g_tests_run++;         if ((a) == (b)) {             std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__                       << " Expected inequality. Both are: " << (a) << std::endl;             g_tests_failed++;             throw std::runtime_error("assertion failed");         } else {             g_tests_passed++;         }     } while (0)

#define ASSERT_THROWS(expr)     do {         g_tests_run++;         bool caught = false;         try { expr; }         catch (...) { caught = true; }         if (!caught) {             std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__                       << " Expected exception but none thrown: " << #expr << std::endl;             g_tests_failed++;             throw std::runtime_error("assertion failed");         } else {             g_tests_passed++;         }     } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<Token> lex(const std::string& src) {
    Lexer lexer(src);
    return lexer.scan();
}

static std::vector<StmtPtr> parse(const std::string& src) {
    auto tokens = lex(src);
    Parser parser(std::move(tokens));
    return parser.parse();
}

static ExprPtr parse_expr(const std::string& src) {
    auto stmts = parse(src);
    ASSERT_EQ(stmts.size(), 1u);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    ASSERT(es != nullptr);
    return std::move(es->expr);
}

// ---------------------------------------------------------------------------
// SECTION 1: Lexer sanity checks
// ---------------------------------------------------------------------------
TEST(lexer_produces_correct_token_types) {
    auto toks = lex("let x = 42");
    ASSERT_EQ(toks.size(), 5u);
    ASSERT_EQ(toks[0].type, TokenType::Let);
    ASSERT_EQ(toks[1].type, TokenType::Identifier);
    ASSERT_EQ(toks[1].lexeme, "x");
    ASSERT_EQ(toks[2].type, TokenType::Equal);
    ASSERT_EQ(toks[3].type, TokenType::Integer);
    ASSERT_EQ(toks[3].lexeme, "42");
    ASSERT_EQ(toks[4].type, TokenType::Eof);
}

TEST(lexer_handles_newlines) {
    auto toks = lex("a\nb");
    ASSERT_EQ(toks.size(), 4u);
    ASSERT_EQ(toks[1].type, TokenType::Newline);
}

TEST(lexer_handles_comments) {
    auto toks = lex("42 // answer");
    ASSERT_EQ(toks.size(), 2u);
    ASSERT_EQ(toks[0].type, TokenType::Integer);
}

TEST(lexer_handles_block_comments) {
    auto toks = lex("/* hello */ 42");
    ASSERT_EQ(toks.size(), 2u);
    ASSERT_EQ(toks[0].type, TokenType::Integer);
}

// ---------------------------------------------------------------------------
// SECTION 2: Literal expressions
// ---------------------------------------------------------------------------
TEST(parse_integer_literal) {
    auto e = parse_expr("42");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::Integer);
    ASSERT_EQ(lit->value, "42");
}

TEST(parse_float_literal) {
    auto e = parse_expr("3.14");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::Float);
    ASSERT_EQ(lit->value, "3.14");
}

TEST(parse_string_literal) {
    auto e = parse_expr("\x22hello\x22");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::String);
    ASSERT_EQ(lit->value, "\x22hello\x22");
}

TEST(parse_boolean_literals) {
    {
        auto e = parse_expr("true");
        auto* lit = dynamic_cast<LiteralExpr*>(e.get());
        ASSERT(lit != nullptr);
        ASSERT_EQ(lit->kind, LiteralExpr::Kind::Bool);
        ASSERT_EQ(lit->value, "true");
    }
    {
        auto e = parse_expr("false");
        auto* lit = dynamic_cast<LiteralExpr*>(e.get());
        ASSERT(lit != nullptr);
        ASSERT_EQ(lit->kind, LiteralExpr::Kind::Bool);
        ASSERT_EQ(lit->value, "false");
    }
}

TEST(parse_nil_literal) {
    auto e = parse_expr("nil");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::Nil);
}

// ---------------------------------------------------------------------------
// SECTION 3: Identifiers & grouping
// ---------------------------------------------------------------------------
TEST(parse_identifier) {
    auto e = parse_expr("foo");
    auto* id = dynamic_cast<IdentExpr*>(e.get());
    ASSERT(id != nullptr);
    ASSERT_EQ(id->name, "foo");
}

TEST(parse_grouped_expression) {
    auto e = parse_expr("(42)");
    auto* g = dynamic_cast<GroupExpr*>(e.get());
    ASSERT(g != nullptr);
    auto* lit = dynamic_cast<LiteralExpr*>(g->expr.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->value, "42");
}

// ---------------------------------------------------------------------------
// SECTION 4: Unary expressions
// ---------------------------------------------------------------------------
TEST(parse_unary_minus) {
    auto e = parse_expr("-5");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    ASSERT(u != nullptr);
    ASSERT_EQ(u->op, "-");
    auto* lit = dynamic_cast<LiteralExpr*>(u->operand.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->value, "5");
}

TEST(parse_unary_bang) {
    auto e = parse_expr("!true");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    ASSERT(u != nullptr);
    ASSERT_EQ(u->op, "!");
}

TEST(parse_unary_tilde) {
    auto e = parse_expr("~0xFF");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    ASSERT(u != nullptr);
    ASSERT_EQ(u->op, "~");
}

TEST(parse_chained_unary) {
    auto e = parse_expr("--5");
    auto* outer = dynamic_cast<UnaryExpr*>(e.get());
    ASSERT(outer != nullptr);
    ASSERT_EQ(outer->op, "-");
    auto* inner = dynamic_cast<UnaryExpr*>(outer->operand.get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->op, "-");
}

// ---------------------------------------------------------------------------
// SECTION 5: RBT expressions
// ---------------------------------------------------------------------------
TEST(parse_prove_rbt) {
    auto e = parse_expr("!Int 42");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->op, RbtExpr::Op::Prove);
    ASSERT_EQ(r->type_name, "Int");
    auto* lit = dynamic_cast<LiteralExpr*>(r->operand.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->value, "42");
}

TEST(parse_mask_rbt) {
    auto e = parse_expr("!~String \x22x\x22");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->op, RbtExpr::Op::Mask);
    ASSERT_EQ(r->type_name, "String");
}

TEST(parse_deep_prove_rbt) {
    auto e = parse_expr("!!Float 3.14");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->op, RbtExpr::Op::DeepProve);
    ASSERT_EQ(r->type_name, "Float");
}

TEST(parse_query_rbt) {
    auto e = parse_expr("?Bool true");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->op, RbtExpr::Op::Query);
    ASSERT_EQ(r->type_name, "Bool");
}

TEST(parse_rbt_with_generic_type) {
    auto e = parse_expr("!List<Int> [1, 2]");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->type_name, "List<Int>");
}

// ---------------------------------------------------------------------------
// SECTION 6: Binary expressions & precedence
// ---------------------------------------------------------------------------
TEST(parse_simple_binary_plus) {
    auto e = parse_expr("1 + 2");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "+");
    auto* l = dynamic_cast<LiteralExpr*>(b->left.get());
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    ASSERT(l != nullptr);
    ASSERT(r != nullptr);
    ASSERT_EQ(l->value, "1");
    ASSERT_EQ(r->value, "2");
}

TEST(parse_simple_binary_mul) {
    auto e = parse_expr("3 * 4");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "*");
}

TEST(precedence_mul_over_plus) {
    auto e = parse_expr("1 + 2 * 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "+");
    auto* l = dynamic_cast<LiteralExpr*>(b->left.get());
    ASSERT(l != nullptr);
    ASSERT_EQ(l->value, "1");
    auto* inner = dynamic_cast<BinaryExpr*>(b->right.get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->op, "*");
}

TEST(precedence_plus_over_eq) {
    auto e = parse_expr("1 + 2 == 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "==");
    auto* left_add = dynamic_cast<BinaryExpr*>(b->left.get());
    ASSERT(left_add != nullptr);
    ASSERT_EQ(left_add->op, "+");
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->value, "3");
}

TEST(precedence_and_over_or) {
    auto e = parse_expr("a || b && c");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "||");
    auto* right_and = dynamic_cast<BinaryExpr*>(b->right.get());
    ASSERT(right_and != nullptr);
    ASSERT_EQ(right_and->op, "&&");
}

TEST(precedence_eq_over_and) {
    auto e = parse_expr("a == b && c == d");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "&&");
    auto* left_eq = dynamic_cast<BinaryExpr*>(b->left.get());
    ASSERT(left_eq != nullptr);
    ASSERT_EQ(left_eq->op, "==");
    auto* right_eq = dynamic_cast<BinaryExpr*>(b->right.get());
    ASSERT(right_eq != nullptr);
    ASSERT_EQ(right_eq->op, "==");
}

TEST(right_associativity_of_eq) {
    auto e = parse_expr("a = b = c");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "=");
    auto* l = dynamic_cast<IdentExpr*>(b->left.get());
    ASSERT(l != nullptr);
    ASSERT_EQ(l->name, "a");
    auto* inner = dynamic_cast<BinaryExpr*>(b->right.get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->op, "=");
}

TEST(left_associativity_of_plus) {
    auto e = parse_expr("1 + 2 + 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "+");
    auto* left_add = dynamic_cast<BinaryExpr*>(b->left.get());
    ASSERT(left_add != nullptr);
    ASSERT_EQ(left_add->op, "+");
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    ASSERT(r != nullptr);
    ASSERT_EQ(r->value, "3");
}

TEST(all_comparison_operators) {
    const char* ops[] = {"==", "!=", "<", ">", "<=", ">="};
    for (const char* op : ops) {
        std::string src = std::string("1 ") + op + " 2";
        auto e = parse_expr(src);
        auto* b = dynamic_cast<BinaryExpr*>(e.get());
        ASSERT(b != nullptr);
        ASSERT_EQ(b->op, op);
    }
}

TEST(all_arithmetic_operators) {
    const char* ops[] = {"+", "-", "*", "/", "%"};
    for (const char* op : ops) {
        std::string src = std::string("1 ") + op + " 2";
        auto e = parse_expr(src);
        auto* b = dynamic_cast<BinaryExpr*>(e.get());
        ASSERT(b != nullptr);
        ASSERT_EQ(b->op, op);
    }
}

// ---------------------------------------------------------------------------
// SECTION 7: Postfix expressions
// ---------------------------------------------------------------------------
TEST(parse_call_no_args) {
    auto e = parse_expr("foo()");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    ASSERT(c != nullptr);
    auto* callee = dynamic_cast<IdentExpr*>(c->callee.get());
    ASSERT(callee != nullptr);
    ASSERT_EQ(callee->name, "foo");
    ASSERT_EQ(c->args.size(), 0u);
}

TEST(parse_call_with_args) {
    auto e = parse_expr("foo(1, 2, 3)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    ASSERT(c != nullptr);
    ASSERT_EQ(c->args.size(), 3u);
    for (size_t i = 0; i < 3; ++i) {
        auto* lit = dynamic_cast<LiteralExpr*>(c->args[i].get());
        ASSERT(lit != nullptr);
        ASSERT_EQ(lit->value, std::to_string(i + 1));
    }
}

TEST(parse_call_with_expression_args) {
    auto e = parse_expr("foo(a + b)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    ASSERT(c != nullptr);
    ASSERT_EQ(c->args.size(), 1u);
    auto* arg = dynamic_cast<BinaryExpr*>(c->args[0].get());
    ASSERT(arg != nullptr);
    ASSERT_EQ(arg->op, "+");
}

TEST(parse_index_expression) {
    auto e = parse_expr("arr[0]");
    auto* idx = dynamic_cast<IndexExpr*>(e.get());
    ASSERT(idx != nullptr);
    auto* target = dynamic_cast<IdentExpr*>(idx->target.get());
    ASSERT(target != nullptr);
    ASSERT_EQ(target->name, "arr");
    auto* index_lit = dynamic_cast<LiteralExpr*>(idx->index.get());
    ASSERT(index_lit != nullptr);
    ASSERT_EQ(index_lit->value, "0");
}

TEST(parse_member_access) {
    auto e = parse_expr("obj.field");
    auto* m = dynamic_cast<MemberExpr*>(e.get());
    ASSERT(m != nullptr);
    ASSERT_EQ(m->name, "field");
    auto* target = dynamic_cast<IdentExpr*>(m->target.get());
    ASSERT(target != nullptr);
    ASSERT_EQ(target->name, "obj");
}

TEST(chained_member_access) {
    auto e = parse_expr("a.b.c");
    auto* outer = dynamic_cast<MemberExpr*>(e.get());
    ASSERT(outer != nullptr);
    ASSERT_EQ(outer->name, "c");
    auto* inner = dynamic_cast<MemberExpr*>(outer->target.get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->name, "b");
    auto* root = dynamic_cast<IdentExpr*>(inner->target.get());
    ASSERT(root != nullptr);
    ASSERT_EQ(root->name, "a");
}

TEST(mixed_postfix_call_then_member) {
    auto e = parse_expr("foo().bar");
    auto* m = dynamic_cast<MemberExpr*>(e.get());
    ASSERT(m != nullptr);
    ASSERT_EQ(m->name, "bar");
    auto* c = dynamic_cast<CallExpr*>(m->target.get());
    ASSERT(c != nullptr);
}

TEST(mixed_postfix_member_then_call) {
    auto e = parse_expr("obj.method(1)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    ASSERT(c != nullptr);
    auto* m = dynamic_cast<MemberExpr*>(c->callee.get());
    ASSERT(m != nullptr);
    ASSERT_EQ(m->name, "method");
}

TEST(call_with_index_arg) {
    auto e = parse_expr("foo(arr[0])");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    ASSERT(c != nullptr);
    ASSERT_EQ(c->args.size(), 1u);
    auto* idx = dynamic_cast<IndexExpr*>(c->args[0].get());
    ASSERT(idx != nullptr);
}

// ---------------------------------------------------------------------------
// SECTION 8: Complex expression combinations
// ---------------------------------------------------------------------------
TEST(unary_plus_binary) {
    auto e = parse_expr("-a + b");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(bop != nullptr);
    ASSERT_EQ(bop->op, "+");
    auto* left_unary = dynamic_cast<UnaryExpr*>(bop->left.get());
    ASSERT(left_unary != nullptr);
    ASSERT_EQ(left_unary->op, "-");
}

TEST(postfix_plus_binary) {
    auto e = parse_expr("a[0] + b[1]");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(bop != nullptr);
    ASSERT_EQ(bop->op, "+");
    auto* left_idx = dynamic_cast<IndexExpr*>(bop->left.get());
    ASSERT(left_idx != nullptr);
    auto* right_idx = dynamic_cast<IndexExpr*>(bop->right.get());
    ASSERT(right_idx != nullptr);
}

TEST(group_overrides_precedence) {
    auto e = parse_expr("(1 + 2) * 3");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(bop != nullptr);
    ASSERT_EQ(bop->op, "*");
    auto* left_group = dynamic_cast<GroupExpr*>(bop->left.get());
    ASSERT(left_group != nullptr);
    auto* inner_add = dynamic_cast<BinaryExpr*>(left_group->expr.get());
    ASSERT(inner_add != nullptr);
    ASSERT_EQ(inner_add->op, "+");
}

TEST(deeply_nested_expression) {
    auto e = parse_expr("a.b[c].d(e, f + g)");
    auto* call = dynamic_cast<CallExpr*>(e.get());
    ASSERT(call != nullptr);
    auto* member_d = dynamic_cast<MemberExpr*>(call->callee.get());
    ASSERT(member_d != nullptr);
    ASSERT_EQ(member_d->name, "d");
    auto* idx = dynamic_cast<IndexExpr*>(member_d->target.get());
    ASSERT(idx != nullptr);
    auto* member_b = dynamic_cast<MemberExpr*>(idx->target.get());
    ASSERT(member_b != nullptr);
    ASSERT_EQ(member_b->name, "b");
}

// ---------------------------------------------------------------------------
// SECTION 9: Statements – let
// ---------------------------------------------------------------------------
TEST(parse_let_without_type) {
    auto stmts = parse("let x = 42");
    ASSERT_EQ(stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    ASSERT(let != nullptr);
    ASSERT_EQ(let->name, "x");
    ASSERT_EQ(let->has_type, false);
    auto* val = dynamic_cast<LiteralExpr*>(let->value.get());
    ASSERT(val != nullptr);
    ASSERT_EQ(val->value, "42");
}

TEST(parse_let_with_type) {
    auto stmts = parse("let x::Int = 42");
    ASSERT_EQ(stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    ASSERT(let != nullptr);
    ASSERT_EQ(let->name, "x");
    ASSERT_EQ(let->has_type, true);
    ASSERT_EQ(let->type_name, "Int");
}

TEST(parse_let_with_generic_type) {
    auto stmts = parse("let xs::List<Int> = nil");
    ASSERT_EQ(stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    ASSERT(let != nullptr);
    ASSERT_EQ(let->type_name, "List<Int>");
}

TEST(parse_let_with_expression_value) {
    auto stmts = parse("let sum = a + b");
    ASSERT_EQ(stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    ASSERT(let != nullptr);
    auto* val = dynamic_cast<BinaryExpr*>(let->value.get());
    ASSERT(val != nullptr);
    ASSERT_EQ(val->op, "+");
}

// ---------------------------------------------------------------------------
// SECTION 10: Statements – block
// ---------------------------------------------------------------------------
TEST(parse_empty_block) {
    auto stmts = parse("{}");
    ASSERT_EQ(stmts.size(), 1u);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 0u);
}

TEST(parse_block_with_single_statement) {
    auto stmts = parse("{ let x = 1 }");
    ASSERT_EQ(stmts.size(), 1u);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(block->stmts[0].get());
    ASSERT(let != nullptr);
    ASSERT_EQ(let->name, "x");
}

TEST(parse_block_with_multiple_statements) {
    auto stmts = parse("{ let x = 1 let y = 2 }");
    ASSERT_EQ(stmts.size(), 1u);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
}

TEST(parse_block_with_newlines) {
    auto stmts = parse("{\n  let x = 1\n  let y = 2\n}");
    ASSERT_EQ(stmts.size(), 1u);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
}

TEST(parse_nested_blocks) {
    auto stmts = parse("{ { let x = 1 } }");
    ASSERT_EQ(stmts.size(), 1u);
    auto* outer = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(outer != nullptr);
    ASSERT_EQ(outer->stmts.size(), 1u);
    auto* inner = dynamic_cast<BlockStmt*>(outer->stmts[0].get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->stmts.size(), 1u);
}

// ---------------------------------------------------------------------------
// SECTION 11: Statements – expression statement
// ---------------------------------------------------------------------------
TEST(parse_expression_statement) {
    auto stmts = parse("foo()");
    ASSERT_EQ(stmts.size(), 1u);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    ASSERT(es != nullptr);
    auto* call = dynamic_cast<CallExpr*>(es->expr.get());
    ASSERT(call != nullptr);
}

TEST(parse_multiple_expression_statements) {
    auto stmts = parse("foo()\nbar()");
    ASSERT_EQ(stmts.size(), 2u);
    auto* es1 = dynamic_cast<ExprStmt*>(stmts[0].get());
    ASSERT(es1 != nullptr);
    auto* es2 = dynamic_cast<ExprStmt*>(stmts[1].get());
    ASSERT(es2 != nullptr);
}

// ---------------------------------------------------------------------------
// SECTION 12: Full program / mixed statements
// ---------------------------------------------------------------------------
TEST(parse_mixed_statements) {
    auto stmts = parse("let x = 1\nfoo()\nlet y = 2");
    ASSERT_EQ(stmts.size(), 3u);
    ASSERT(dynamic_cast<LetStmt*>(stmts[0].get()) != nullptr);
    ASSERT(dynamic_cast<ExprStmt*>(stmts[1].get()) != nullptr);
    ASSERT(dynamic_cast<LetStmt*>(stmts[2].get()) != nullptr);
}

TEST(parse_let_inside_block) {
    auto stmts = parse("let outer = 1 { let inner = 2 }");
    ASSERT_EQ(stmts.size(), 2u);
    ASSERT(dynamic_cast<LetStmt*>(stmts[0].get()) != nullptr);
    ASSERT(dynamic_cast<BlockStmt*>(stmts[1].get()) != nullptr);
}

TEST(parse_block_as_expression_value) {
    auto stmts = parse("let x = { 42 }");
    ASSERT_EQ(stmts.size(), 1u);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    ASSERT(let != nullptr);
    auto* block = dynamic_cast<BlockStmt*>(let->value.get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
}

// ---------------------------------------------------------------------------
// SECTION 13: Newline handling inside delimiters
// ---------------------------------------------------------------------------
TEST(newlines_ignored_inside_parens) {
    auto e = parse_expr("(\n  1 + 2\n)");
    auto* g = dynamic_cast<GroupExpr*>(e.get());
    ASSERT(g != nullptr);
    auto* inner = dynamic_cast<BinaryExpr*>(g->expr.get());
    ASSERT(inner != nullptr);
    ASSERT_EQ(inner->op, "+");
}

TEST(newlines_ignored_inside_brackets) {
    auto e = parse_expr("arr[\n  0\n]");
    auto* idx = dynamic_cast<IndexExpr*>(e.get());
    ASSERT(idx != nullptr);
}

TEST(newlines_ignored_inside_braces) {
    auto stmts = parse("{\n  let x = 1\n  let y = 2\n}");
    ASSERT_EQ(stmts.size(), 1u);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    ASSERT(block != nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
}

TEST(newlines_separate_top_level_statements) {
    auto stmts = parse("foo()\nbar()");
    ASSERT_EQ(stmts.size(), 2u);
}

// ---------------------------------------------------------------------------
// SECTION 14: Error cases
// ---------------------------------------------------------------------------
TEST(error_unexpected_token_in_expression) {
    auto tokens = lex("let");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_missing_closing_paren) {
    auto tokens = lex("(1 + 2");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_missing_closing_bracket) {
    auto tokens = lex("arr[0");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_missing_closing_brace) {
    auto tokens = lex("{ let x = 1");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_let_missing_variable_name) {
    auto tokens = lex("let = 1");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_let_missing_equals) {
    auto tokens = lex("let x 1");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_member_access_without_name) {
    auto tokens = lex("obj.");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_call_missing_closing_paren) {
    auto tokens = lex("foo(1, 2");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_unexpected_end_of_input) {
    auto tokens = lex("-");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(error_empty_input) {
    auto tokens = lex("");
    Parser parser(std::move(tokens));
    auto stmts = parser.parse();
    ASSERT_EQ(stmts.size(), 0u);
}

// ---------------------------------------------------------------------------
// SECTION 15: Edge cases
// ---------------------------------------------------------------------------
TEST(parse_single_identifier_as_statement) {
    auto stmts = parse("x");
    ASSERT_EQ(stmts.size(), 1u);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    ASSERT(es != nullptr);
    auto* id = dynamic_cast<IdentExpr*>(es->expr.get());
    ASSERT(id != nullptr);
    ASSERT_EQ(id->name, "x");
}

TEST(parse_single_literal_as_statement) {
    auto stmts = parse("42");
    ASSERT_EQ(stmts.size(), 1u);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    ASSERT(es != nullptr);
    auto* lit = dynamic_cast<LiteralExpr*>(es->expr.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->value, "42");
}

TEST(parse_deeply_nested_unary) {
    auto e = parse_expr("!!!!x");
    auto* u1 = dynamic_cast<UnaryExpr*>(e.get());
    ASSERT(u1 != nullptr);
    auto* u2 = dynamic_cast<UnaryExpr*>(u1->operand.get());
    ASSERT(u2 != nullptr);
    auto* u3 = dynamic_cast<UnaryExpr*>(u2->operand.get());
    ASSERT(u3 != nullptr);
    auto* u4 = dynamic_cast<UnaryExpr*>(u3->operand.get());
    ASSERT(u4 != nullptr);
    auto* id = dynamic_cast<IdentExpr*>(u4->operand.get());
    ASSERT(id != nullptr);
    ASSERT_EQ(id->name, "x");
}

TEST(parse_long_binary_chain) {
    auto e = parse_expr("1 + 2 + 3 + 4 + 5");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "+");
    auto* left = dynamic_cast<BinaryExpr*>(b->left.get());
    ASSERT(left != nullptr);
    ASSERT_EQ(left->op, "+");
}

TEST(parse_scientific_notation_float) {
    auto e = parse_expr("1e10");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::Float);
    ASSERT_EQ(lit->value, "1e10");
}

TEST(parse_negative_scientific_notation) {
    auto e = parse_expr("1.5e-3");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    ASSERT(lit != nullptr);
    ASSERT_EQ(lit->kind, LiteralExpr::Kind::Float);
    ASSERT_EQ(lit->value, "1.5e-3");
}

TEST(parse_empty_call_with_trailing_comma_error) {
    auto tokens = lex("foo(1,)");
    Parser parser(std::move(tokens));
    ASSERT_THROWS(parser.parse());
}

TEST(parse_multiple_newlines_between_statements) {
    auto stmts = parse("foo()\n\n\nbar()");
    ASSERT_EQ(stmts.size(), 2u);
}

TEST(parse_whitespace_only_input) {
    auto stmts = parse("   \n   \n   ");
    ASSERT_EQ(stmts.size(), 0u);
}

TEST(parse_comment_only_input) {
    auto stmts = parse("// nothing here");
    ASSERT_EQ(stmts.size(), 0u);
}

TEST(parse_block_comment_in_expression) {
    auto e = parse_expr("1 /* middle */ + 2");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    ASSERT(b != nullptr);
    ASSERT_EQ(b->op, "+");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== Running parser tests ===" << std::endl;
    std::cout << "Registered tests: " << test_registry().size() << std::endl;
    std::cout << std::endl;

    for (const auto& t : test_registry()) {
        std::cout << "[RUN ] " << t.name << std::endl;
        try {
            t.func();
            std::cout << "[PASS] " << t.name << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << " - " << e.what() << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "===========================" << std::endl;
    std::cout << "Total assertions: " << g_tests_run << std::endl;
    std::cout << "Passed:           " << g_tests_passed << std::endl;
    std::cout << "Failed:           " << g_tests_failed << std::endl;
    std::cout << "===========================" << std::endl;

    return g_tests_failed > 0 ? 1 : 0;
}
