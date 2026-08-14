#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "lexer.h"
#include "parser.h"
#include "ast.h"

#include <memory>
#include <string>
#include <vector>

using namespace bang;
using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// Helper: build token vector from source
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
    auto tokens = lex(src);
    // Append EOF if not present
    Parser parser(std::move(tokens));
    auto stmts = parser.parse();
    REQUIRE(stmts.size() == 1);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    REQUIRE(es != nullptr);
    return std::move(es->expr);
}

// ---------------------------------------------------------------------------
// SECTION 1: Lexer sanity checks (minimal – just enough to trust tokens)
// ---------------------------------------------------------------------------
TEST_CASE("Lexer produces correct token types", "[lexer]") {
    auto toks = lex("let x = 42");
    REQUIRE(toks.size() == 5); // let, x, =, 42, EOF
    CHECK(toks[0].type == TokenType::Let);
    CHECK(toks[1].type == TokenType::Identifier);
    CHECK(toks[1].lexeme == "x");
    CHECK(toks[2].type == TokenType::Equal);
    CHECK(toks[3].type == TokenType::Integer);
    CHECK(toks[3].lexeme == "42");
    CHECK(toks[4].type == TokenType::Eof);
}

TEST_CASE("Lexer handles newlines", "[lexer]") {
    auto toks = lex("a\nb");
    REQUIRE(toks.size() == 4); // a, newline, b, EOF
    CHECK(toks[1].type == TokenType::Newline);
}

TEST_CASE("Lexer handles comments", "[lexer]") {
    auto toks = lex("42 // answer");
    REQUIRE(toks.size() == 2); // 42, EOF (comment skipped)
    CHECK(toks[0].type == TokenType::Integer);
}

TEST_CASE("Lexer handles block comments", "[lexer]") {
    auto toks = lex("/* hello */ 42");
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].type == TokenType::Integer);
}

// ---------------------------------------------------------------------------
// SECTION 2: Literal expressions
// ---------------------------------------------------------------------------
TEST_CASE("Parse integer literal", "[expr][literal]") {
    auto e = parse_expr("42");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::Integer);
    CHECK(lit->value == "42");
}

TEST_CASE("Parse float literal", "[expr][literal]") {
    auto e = parse_expr("3.14");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::Float);
    CHECK(lit->value == "3.14");
}

TEST_CASE("Parse string literal", "[expr][literal]") {
    auto e = parse_expr("\"hello\"");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::String);
    CHECK(lit->value == "\"hello\"");
}

TEST_CASE("Parse boolean literals", "[expr][literal]") {
    {
        auto e = parse_expr("true");
        auto* lit = dynamic_cast<LiteralExpr*>(e.get());
        REQUIRE(lit != nullptr);
        CHECK(lit->kind == LiteralExpr::Kind::Bool);
        CHECK(lit->value == "true");
    }
    {
        auto e = parse_expr("false");
        auto* lit = dynamic_cast<LiteralExpr*>(e.get());
        REQUIRE(lit != nullptr);
        CHECK(lit->kind == LiteralExpr::Kind::Bool);
        CHECK(lit->value == "false");
    }
}

TEST_CASE("Parse nil literal", "[expr][literal]") {
    auto e = parse_expr("nil");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::Nil);
}

// ---------------------------------------------------------------------------
// SECTION 3: Identifiers & grouping
// ---------------------------------------------------------------------------
TEST_CASE("Parse identifier", "[expr][ident]") {
    auto e = parse_expr("foo");
    auto* id = dynamic_cast<IdentExpr*>(e.get());
    REQUIRE(id != nullptr);
    CHECK(id->name == "foo");
}

TEST_CASE("Parse grouped expression", "[expr][group]") {
    auto e = parse_expr("(42)");
    auto* g = dynamic_cast<GroupExpr*>(e.get());
    REQUIRE(g != nullptr);
    auto* lit = dynamic_cast<LiteralExpr*>(g->expr.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->value == "42");
}

// ---------------------------------------------------------------------------
// SECTION 4: Unary expressions
// ---------------------------------------------------------------------------
TEST_CASE("Parse unary minus", "[expr][unary]") {
    auto e = parse_expr("-5");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    REQUIRE(u != nullptr);
    CHECK(u->op == "-");
    auto* lit = dynamic_cast<LiteralExpr*>(u->operand.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->value == "5");
}

TEST_CASE("Parse unary bang", "[expr][unary]") {
    auto e = parse_expr("!true");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    REQUIRE(u != nullptr);
    CHECK(u->op == "!");
}

TEST_CASE("Parse unary tilde", "[expr][unary]") {
    auto e = parse_expr("~0xFF");
    auto* u = dynamic_cast<UnaryExpr*>(e.get());
    REQUIRE(u != nullptr);
    CHECK(u->op == "~");
}

TEST_CASE("Parse chained unary", "[expr][unary]") {
    auto e = parse_expr("--5");
    auto* outer = dynamic_cast<UnaryExpr*>(e.get());
    REQUIRE(outer != nullptr);
    CHECK(outer->op == "-");
    auto* inner = dynamic_cast<UnaryExpr*>(outer->operand.get());
    REQUIRE(inner != nullptr);
    CHECK(inner->op == "-");
}

// ---------------------------------------------------------------------------
// SECTION 5: RBT (Runtime Bounded Type) expressions
// ---------------------------------------------------------------------------
TEST_CASE("Parse prove (!T)", "[expr][rbt]") {
    auto e = parse_expr("!Int 42");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    REQUIRE(r != nullptr);
    CHECK(r->op == RbtExpr::Op::Prove);
    CHECK(r->type_name == "Int");
    auto* lit = dynamic_cast<LiteralExpr*>(r->operand.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->value == "42");
}

TEST_CASE("Parse mask (!~T)", "[expr][rbt]") {
    auto e = parse_expr("!~String \"x\"");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    REQUIRE(r != nullptr);
    CHECK(r->op == RbtExpr::Op::Mask);
    CHECK(r->type_name == "String");
}

TEST_CASE("Parse deep prove (!!T)", "[expr][rbt]") {
    auto e = parse_expr("!!Float 3.14");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    REQUIRE(r != nullptr);
    CHECK(r->op == RbtExpr::Op::DeepProve);
    CHECK(r->type_name == "Float");
}

TEST_CASE("Parse query (?T)", "[expr][rbt]") {
    auto e = parse_expr("?Bool true");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    REQUIRE(r != nullptr);
    CHECK(r->op == RbtExpr::Op::Query);
    CHECK(r->type_name == "Bool");
}

TEST_CASE("Parse RBT with generic type", "[expr][rbt]") {
    auto e = parse_expr("!List<Int> [1, 2]");
    auto* r = dynamic_cast<RbtExpr*>(e.get());
    REQUIRE(r != nullptr);
    CHECK(r->type_name == "List<Int>");
}

// ---------------------------------------------------------------------------
// SECTION 6: Binary expressions & precedence
// ---------------------------------------------------------------------------
TEST_CASE("Parse simple binary +", "[expr][binary]") {
    auto e = parse_expr("1 + 2");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "+");
    auto* l = dynamic_cast<LiteralExpr*>(b->left.get());
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    REQUIRE(l != nullptr);
    REQUIRE(r != nullptr);
    CHECK(l->value == "1");
    CHECK(r->value == "2");
}

TEST_CASE("Parse simple binary *", "[expr][binary]") {
    auto e = parse_expr("3 * 4");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "*");
}

TEST_CASE("Precedence: * over +", "[expr][binary][precedence]") {
    auto e = parse_expr("1 + 2 * 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "+");
    // left must be 1
    auto* l = dynamic_cast<LiteralExpr*>(b->left.get());
    REQUIRE(l != nullptr);
    CHECK(l->value == "1");
    // right must be (2 * 3)
    auto* inner = dynamic_cast<BinaryExpr*>(b->right.get());
    REQUIRE(inner != nullptr);
    CHECK(inner->op == "*");
}

TEST_CASE("Precedence: + over ==", "[expr][binary][precedence]") {
    auto e = parse_expr("1 + 2 == 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "==");
    // left must be (1 + 2)
    auto* left_add = dynamic_cast<BinaryExpr*>(b->left.get());
    REQUIRE(left_add != nullptr);
    CHECK(left_add->op == "+");
    // right must be 3
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    REQUIRE(r != nullptr);
    CHECK(r->value == "3");
}

TEST_CASE("Precedence: && over ||", "[expr][binary][precedence]") {
    auto e = parse_expr("a || b && c");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "||");
    auto* right_and = dynamic_cast<BinaryExpr*>(b->right.get());
    REQUIRE(right_and != nullptr);
    CHECK(right_and->op == "&&");
}

TEST_CASE("Precedence: == over &&", "[expr][binary][precedence]") {
    auto e = parse_expr("a == b && c == d");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "&&");
    auto* left_eq = dynamic_cast<BinaryExpr*>(b->left.get());
    REQUIRE(left_eq != nullptr);
    CHECK(left_eq->op == "==");
    auto* right_eq = dynamic_cast<BinaryExpr*>(b->right.get());
    REQUIRE(right_eq != nullptr);
    CHECK(right_eq->op == "==");
}

TEST_CASE("Right associativity of =", "[expr][binary][associativity]") {
    auto e = parse_expr("a = b = c");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "=");
    // left is a
    auto* l = dynamic_cast<IdentExpr*>(b->left.get());
    REQUIRE(l != nullptr);
    CHECK(l->name == "a");
    // right is (b = c)
    auto* inner = dynamic_cast<BinaryExpr*>(b->right.get());
    REQUIRE(inner != nullptr);
    CHECK(inner->op == "=");
}

TEST_CASE("Left associativity of +", "[expr][binary][associativity]") {
    auto e = parse_expr("1 + 2 + 3");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "+");
    // left is (1 + 2)
    auto* left_add = dynamic_cast<BinaryExpr*>(b->left.get());
    REQUIRE(left_add != nullptr);
    CHECK(left_add->op == "+");
    // right is 3
    auto* r = dynamic_cast<LiteralExpr*>(b->right.get());
    REQUIRE(r != nullptr);
    CHECK(r->value == "3");
}

TEST_CASE("All comparison operators", "[expr][binary]") {
    for (const char* op : {"==", "!=", "<", ">", "<=", ">="}) {
        auto src = std::string("1 ") + op + " 2";
        auto e = parse_expr(src);
        auto* b = dynamic_cast<BinaryExpr*>(e.get());
        REQUIRE(b != nullptr);
        CHECK(b->op == op);
    }
}

TEST_CASE("All arithmetic operators", "[expr][binary]") {
    for (const char* op : {"+", "-", "*", "/", "%"}) {
        auto src = std::string("1 ") + op + " 2";
        auto e = parse_expr(src);
        auto* b = dynamic_cast<BinaryExpr*>(e.get());
        REQUIRE(b != nullptr);
        CHECK(b->op == op);
    }
}

// ---------------------------------------------------------------------------
// SECTION 7: Postfix expressions
// ---------------------------------------------------------------------------
TEST_CASE("Parse function call no args", "[expr][call]") {
    auto e = parse_expr("foo()");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(c != nullptr);
    auto* callee = dynamic_cast<IdentExpr*>(c->callee.get());
    REQUIRE(callee != nullptr);
    CHECK(callee->name == "foo");
    CHECK(c->args.empty());
}

TEST_CASE("Parse function call with args", "[expr][call]") {
    auto e = parse_expr("foo(1, 2, 3)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(c != nullptr);
    REQUIRE(c->args.size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        auto* lit = dynamic_cast<LiteralExpr*>(c->args[i].get());
        REQUIRE(lit != nullptr);
        CHECK(lit->value == std::to_string(i + 1));
    }
}

TEST_CASE("Parse function call with expression args", "[expr][call]") {
    auto e = parse_expr("foo(a + b)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(c != nullptr);
    REQUIRE(c->args.size() == 1);
    auto* arg = dynamic_cast<BinaryExpr*>(c->args[0].get());
    REQUIRE(arg != nullptr);
    CHECK(arg->op == "+");
}

TEST_CASE("Parse index expression", "[expr][index]") {
    auto e = parse_expr("arr[0]");
    auto* idx = dynamic_cast<IndexExpr*>(e.get());
    REQUIRE(idx != nullptr);
    auto* target = dynamic_cast<IdentExpr*>(idx->target.get());
    REQUIRE(target != nullptr);
    CHECK(target->name == "arr");
    auto* index_lit = dynamic_cast<LiteralExpr*>(idx->index.get());
    REQUIRE(index_lit != nullptr);
    CHECK(index_lit->value == "0");
}

TEST_CASE("Parse member access", "[expr][member]") {
    auto e = parse_expr("obj.field");
    auto* m = dynamic_cast<MemberExpr*>(e.get());
    REQUIRE(m != nullptr);
    CHECK(m->name == "field");
    auto* target = dynamic_cast<IdentExpr*>(m->target.get());
    REQUIRE(target != nullptr);
    CHECK(target->name == "obj");
}

TEST_CASE("Chained member access", "[expr][member]") {
    auto e = parse_expr("a.b.c");
    auto* outer = dynamic_cast<MemberExpr*>(e.get());
    REQUIRE(outer != nullptr);
    CHECK(outer->name == "c");
    auto* inner = dynamic_cast<MemberExpr*>(outer->target.get());
    REQUIRE(inner != nullptr);
    CHECK(inner->name == "b");
    auto* root = dynamic_cast<IdentExpr*>(inner->target.get());
    REQUIRE(root != nullptr);
    CHECK(root->name == "a");
}

TEST_CASE("Mixed postfix: call then member", "[expr][postfix]") {
    auto e = parse_expr("foo().bar");
    auto* m = dynamic_cast<MemberExpr*>(e.get());
    REQUIRE(m != nullptr);
    CHECK(m->name == "bar");
    auto* c = dynamic_cast<CallExpr*>(m->target.get());
    REQUIRE(c != nullptr);
}

TEST_CASE("Mixed postfix: member then call", "[expr][postfix]") {
    auto e = parse_expr("obj.method(1)");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(c != nullptr);
    auto* m = dynamic_cast<MemberExpr*>(c->callee.get());
    REQUIRE(m != nullptr);
    CHECK(m->name == "method");
}

TEST_CASE("Call with index arg", "[expr][postfix]") {
    auto e = parse_expr("foo(arr[0])");
    auto* c = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(c != nullptr);
    REQUIRE(c->args.size() == 1);
    auto* idx = dynamic_cast<IndexExpr*>(c->args[0].get());
    REQUIRE(idx != nullptr);
}

// ---------------------------------------------------------------------------
// SECTION 8: Complex expression combinations
// ---------------------------------------------------------------------------
TEST_CASE("Unary + binary", "[expr][complex]") {
    auto e = parse_expr("-a + b");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(bop != nullptr);
    CHECK(bop->op == "+");
    auto* left_unary = dynamic_cast<UnaryExpr*>(bop->left.get());
    REQUIRE(left_unary != nullptr);
    CHECK(left_unary->op == "-");
}

TEST_CASE("Postfix + binary", "[expr][complex]") {
    auto e = parse_expr("a[0] + b[1]");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(bop != nullptr);
    CHECK(bop->op == "+");
    auto* left_idx = dynamic_cast<IndexExpr*>(bop->left.get());
    REQUIRE(left_idx != nullptr);
    auto* right_idx = dynamic_cast<IndexExpr*>(bop->right.get());
    REQUIRE(right_idx != nullptr);
}

TEST_CASE("Group overrides precedence", "[expr][complex]") {
    auto e = parse_expr("(1 + 2) * 3");
    auto* bop = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(bop != nullptr);
    CHECK(bop->op == "*");
    auto* left_group = dynamic_cast<GroupExpr*>(bop->left.get());
    REQUIRE(left_group != nullptr);
    auto* inner_add = dynamic_cast<BinaryExpr*>(left_group->expr.get());
    REQUIRE(inner_add != nullptr);
    CHECK(inner_add->op == "+");
}

TEST_CASE("Deeply nested expression", "[expr][complex]") {
    auto e = parse_expr("a.b[c].d(e, f + g)");
    // Should parse as: ((a.b)[c]).d(e, f + g)
    auto* call = dynamic_cast<CallExpr*>(e.get());
    REQUIRE(call != nullptr);
    auto* member_d = dynamic_cast<MemberExpr*>(call->callee.get());
    REQUIRE(member_d != nullptr);
    CHECK(member_d->name == "d");
    auto* idx = dynamic_cast<IndexExpr*>(member_d->target.get());
    REQUIRE(idx != nullptr);
    auto* member_b = dynamic_cast<MemberExpr*>(idx->target.get());
    REQUIRE(member_b != nullptr);
    CHECK(member_b->name == "b");
}

// ---------------------------------------------------------------------------
// SECTION 9: Statements – let
// ---------------------------------------------------------------------------
TEST_CASE("Parse let without type", "[stmt][let]") {
    auto stmts = parse("let x = 42");
    REQUIRE(stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    REQUIRE(let != nullptr);
    CHECK(let->name == "x");
    CHECK(let->has_type == false);
    auto* val = dynamic_cast<LiteralExpr*>(let->value.get());
    REQUIRE(val != nullptr);
    CHECK(val->value == "42");
}

TEST_CASE("Parse let with type", "[stmt][let]") {
    auto stmts = parse("let x::Int = 42");
    REQUIRE(stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    REQUIRE(let != nullptr);
    CHECK(let->name == "x");
    CHECK(let->has_type == true);
    CHECK(let->type_name == "Int");
}

TEST_CASE("Parse let with generic type", "[stmt][let]") {
    auto stmts = parse("let xs::List<Int> = nil");
    REQUIRE(stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    REQUIRE(let != nullptr);
    CHECK(let->type_name == "List<Int>");
}

TEST_CASE("Parse let with expression value", "[stmt][let]") {
    auto stmts = parse("let sum = a + b");
    REQUIRE(stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    REQUIRE(let != nullptr);
    auto* val = dynamic_cast<BinaryExpr*>(let->value.get());
    REQUIRE(val != nullptr);
    CHECK(val->op == "+");
}

// ---------------------------------------------------------------------------
// SECTION 10: Statements – block
// ---------------------------------------------------------------------------
TEST_CASE("Parse empty block", "[stmt][block]") {
    auto stmts = parse("{}");
    REQUIRE(stmts.size() == 1);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(block != nullptr);
    CHECK(block->stmts.empty());
}

TEST_CASE("Parse block with single statement", "[stmt][block]") {
    auto stmts = parse("{ let x = 1 }");
    REQUIRE(stmts.size() == 1);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(block != nullptr);
    REQUIRE(block->stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(block->stmts[0].get());
    REQUIRE(let != nullptr);
    CHECK(let->name == "x");
}

TEST_CASE("Parse block with multiple statements", "[stmt][block]") {
    auto stmts = parse("{ let x = 1 let y = 2 }");
    REQUIRE(stmts.size() == 1);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(block != nullptr);
    REQUIRE(block->stmts.size() == 2);
}

TEST_CASE("Parse block with newlines", "[stmt][block]") {
    auto stmts = parse("{\n  let x = 1\n  let y = 2\n}");
    REQUIRE(stmts.size() == 1);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(block != nullptr);
    REQUIRE(block->stmts.size() == 2);
}

TEST_CASE("Parse nested blocks", "[stmt][block]") {
    auto stmts = parse("{ { let x = 1 } }");
    REQUIRE(stmts.size() == 1);
    auto* outer = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(outer != nullptr);
    REQUIRE(outer->stmts.size() == 1);
    auto* inner = dynamic_cast<BlockStmt*>(outer->stmts[0].get());
    REQUIRE(inner != nullptr);
    REQUIRE(inner->stmts.size() == 1);
}

// ---------------------------------------------------------------------------
// SECTION 11: Statements – expression statement
// ---------------------------------------------------------------------------
TEST_CASE("Parse expression statement", "[stmt][exprstmt]") {
    auto stmts = parse("foo()");
    REQUIRE(stmts.size() == 1);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    REQUIRE(es != nullptr);
    auto* call = dynamic_cast<CallExpr*>(es->expr.get());
    REQUIRE(call != nullptr);
}

TEST_CASE("Parse multiple expression statements", "[stmt][exprstmt]") {
    auto stmts = parse("foo()\nbar()");
    REQUIRE(stmts.size() == 2);
    auto* es1 = dynamic_cast<ExprStmt*>(stmts[0].get());
    REQUIRE(es1 != nullptr);
    auto* es2 = dynamic_cast<ExprStmt*>(stmts[1].get());
    REQUIRE(es2 != nullptr);
}

// ---------------------------------------------------------------------------
// SECTION 12: Full program / mixed statements
// ---------------------------------------------------------------------------
TEST_CASE("Parse mixed statements", "[program]") {
    auto stmts = parse("let x = 1\nfoo()\nlet y = 2");
    REQUIRE(stmts.size() == 3);
    CHECK(dynamic_cast<LetStmt*>(stmts[0].get()) != nullptr);
    CHECK(dynamic_cast<ExprStmt*>(stmts[1].get()) != nullptr);
    CHECK(dynamic_cast<LetStmt*>(stmts[2].get()) != nullptr);
}

TEST_CASE("Parse let inside block", "[program]") {
    auto stmts = parse("let outer = 1 { let inner = 2 }");
    REQUIRE(stmts.size() == 2);
    CHECK(dynamic_cast<LetStmt*>(stmts[0].get()) != nullptr);
    CHECK(dynamic_cast<BlockStmt*>(stmts[1].get()) != nullptr);
}

TEST_CASE("Parse block as expression value", "[program]") {
    auto stmts = parse("let x = { 42 }");
    REQUIRE(stmts.size() == 1);
    auto* let = dynamic_cast<LetStmt*>(stmts[0].get());
    REQUIRE(let != nullptr);
    auto* block = dynamic_cast<BlockStmt*>(let->value.get());
    REQUIRE(block != nullptr);
    REQUIRE(block->stmts.size() == 1);
}

// ---------------------------------------------------------------------------
// SECTION 13: Newline handling inside delimiters
// ---------------------------------------------------------------------------
TEST_CASE("Newlines ignored inside parentheses", "[newline]") {
    auto e = parse_expr("(\n  1 + 2\n)");
    auto* g = dynamic_cast<GroupExpr*>(e.get());
    REQUIRE(g != nullptr);
    auto* inner = dynamic_cast<BinaryExpr*>(g->expr.get());
    REQUIRE(inner != nullptr);
    CHECK(inner->op == "+");
}

TEST_CASE("Newlines ignored inside brackets", "[newline]") {
    auto e = parse_expr("arr[\n  0\n]");
    auto* idx = dynamic_cast<IndexExpr*>(e.get());
    REQUIRE(idx != nullptr);
}

TEST_CASE("Newlines ignored inside braces", "[newline]") {
    auto stmts = parse("{\n  let x = 1\n  let y = 2\n}");
    REQUIRE(stmts.size() == 1);
    auto* block = dynamic_cast<BlockStmt*>(stmts[0].get());
    REQUIRE(block != nullptr);
    REQUIRE(block->stmts.size() == 2);
}

TEST_CASE("Newlines separate top-level statements", "[newline]") {
    auto stmts = parse("foo()\nbar()");
    REQUIRE(stmts.size() == 2);
}

// ---------------------------------------------------------------------------
// SECTION 14: Error cases
// ---------------------------------------------------------------------------
TEST_CASE("Error: unexpected token in expression", "[error]") {
    auto tokens = lex("let");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: missing closing paren", "[error]") {
    auto tokens = lex("(1 + 2");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: missing closing bracket", "[error]") {
    auto tokens = lex("arr[0");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: missing closing brace", "[error]") {
    auto tokens = lex("{ let x = 1");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: let missing variable name", "[error]") {
    auto tokens = lex("let = 1");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: let missing equals", "[error]") {
    auto tokens = lex("let x 1");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: member access without name", "[error]") {
    auto tokens = lex("obj.");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: call missing closing paren", "[error]") {
    auto tokens = lex("foo(1, 2");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: unexpected end of input", "[error]") {
    auto tokens = lex("-");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Error: empty input", "[error]") {
    auto tokens = lex("");
    Parser parser(std::move(tokens));
    auto stmts = parser.parse();
    CHECK(stmts.empty());
}

// ---------------------------------------------------------------------------
// SECTION 15: Edge cases
// ---------------------------------------------------------------------------
TEST_CASE("Parse single identifier as statement", "[edge]") {
    auto stmts = parse("x");
    REQUIRE(stmts.size() == 1);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    REQUIRE(es != nullptr);
    auto* id = dynamic_cast<IdentExpr*>(es->expr.get());
    REQUIRE(id != nullptr);
    CHECK(id->name == "x");
}

TEST_CASE("Parse single literal as statement", "[edge]") {
    auto stmts = parse("42");
    REQUIRE(stmts.size() == 1);
    auto* es = dynamic_cast<ExprStmt*>(stmts[0].get());
    REQUIRE(es != nullptr);
    auto* lit = dynamic_cast<LiteralExpr*>(es->expr.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->value == "42");
}

TEST_CASE("Parse deeply nested unary", "[edge]") {
    auto e = parse_expr("!!!!x");
    auto* u1 = dynamic_cast<UnaryExpr*>(e.get());
    REQUIRE(u1 != nullptr);
    auto* u2 = dynamic_cast<UnaryExpr*>(u1->operand.get());
    REQUIRE(u2 != nullptr);
    auto* u3 = dynamic_cast<UnaryExpr*>(u2->operand.get());
    REQUIRE(u3 != nullptr);
    auto* u4 = dynamic_cast<UnaryExpr*>(u3->operand.get());
    REQUIRE(u4 != nullptr);
    auto* id = dynamic_cast<IdentExpr*>(u4->operand.get());
    REQUIRE(id != nullptr);
    CHECK(id->name == "x");
}

TEST_CASE("Parse long binary chain", "[edge]") {
    auto e = parse_expr("1 + 2 + 3 + 4 + 5");
    // Left-associative, so tree leans left
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "+");
    // Drill down left side
    auto* left = dynamic_cast<BinaryExpr*>(b->left.get());
    REQUIRE(left != nullptr);
    CHECK(left->op == "+");
}

TEST_CASE("Parse scientific notation float", "[edge]") {
    auto e = parse_expr("1e10");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::Float);
    CHECK(lit->value == "1e10");
}

TEST_CASE("Parse negative scientific notation", "[edge]") {
    auto e = parse_expr("1.5e-3");
    auto* lit = dynamic_cast<LiteralExpr*>(e.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->kind == LiteralExpr::Kind::Float);
    CHECK(lit->value == "1.5e-3");
}

TEST_CASE("Parse empty call with trailing comma (error)", "[edge][error]") {
    // Parser does NOT allow trailing comma: foo(1,)
    auto tokens = lex("foo(1,)");
    Parser parser(std::move(tokens));
    REQUIRE_THROWS_AS(parser.parse(), std::runtime_error);
}

TEST_CASE("Parse multiple newlines between statements", "[edge]") {
    auto stmts = parse("foo()\n\n\nbar()");
    REQUIRE(stmts.size() == 2);
}

TEST_CASE("Parse whitespace-only input", "[edge]") {
    auto stmts = parse("   \n   \n   ");
    CHECK(stmts.empty());
}

TEST_CASE("Parse comment-only input", "[edge]") {
    auto stmts = parse("// nothing here");
    CHECK(stmts.empty());
}

TEST_CASE("Parse block comment in expression", "[edge]") {
    auto e = parse_expr("1 /* middle */ + 2");
    auto* b = dynamic_cast<BinaryExpr*>(e.get());
    REQUIRE(b != nullptr);
    CHECK(b->op == "+");
}
