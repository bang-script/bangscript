#include <iostream>
#include <cassert>
#include "semantic.h"
#include "parser.h"
#include "lexer.h"

using namespace bang;

// ============================================================================
// TEST HELPERS
// ============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string error_msg;
};

std::vector<TestResult> results;

void print_diagnostics(const SemanticAnalyzer& analyzer) {
    for (const auto& e : analyzer.errors()) {
        std::cerr << "  error at " << e.line << ":" << e.column
                  << ": " << e.message << "\n";
    }
    for (const auto& w : analyzer.warnings()) {
        std::cerr << "  warning at " << w.line << ":" << w.column
                  << ": " << w.message << "\n";
    }
}

bool run_test(const std::string& name, const std::string& source, bool expect_success) {
    std::cout << "[TEST] " << name << " ... ";

    Lexer lexer(source);
    auto tokens = lexer.lex();

    if (!lexer.errors().empty()) {
        std::cerr << "LEXER ERRORS:\n";
        for (const auto& e : lexer.errors()) std::cerr << "  " << e << "\n";
        results.push_back({name, false, "lexer errors"});
        std::cout << "FAIL (lexer)\n";
        return false;
    }

    Parser parser(std::move(tokens));
    auto stmts = parser.parse();

    if (!parser.errors().empty()) {
        std::cerr << "PARSER ERRORS:\n";
        for (const auto& e : parser.errors()) std::cerr << "  " << e << "\n";
        results.push_back({name, false, "parser errors"});
        std::cout << "FAIL (parser)\n";
        return false;
    }

    SemanticAnalyzer analyzer;
    bool ok = analyzer.analyze(stmts);

    bool passed = (ok == expect_success);

    if (!passed) {
        std::cerr << "\n";
        print_diagnostics(analyzer);
    }

    results.push_back({name, passed, ""});
    std::cout << (passed ? "PASS" : "FAIL") << "\n";
    return passed;
}

// ============================================================================
// TESTS
// ============================================================================

void run_all_tests() {
    int passed = 0, total = 0;

    // ---- BASIC TYPE CHECKING ----

    total++;
    if (run_test("let with integer literal",
        "let x = 42\n",
        true)) passed++;

    total++;
    if (run_test("let with correct annotation",
        "let x :: Integer = 42\n",
        true)) passed++;

    total++;
    if (run_test("let with wrong annotation",
        "let x :: String = 42\n",
        false)) passed++;

    total++;
    if (run_test("undefined variable",
        "output(x)\n",
        false)) passed++;

    // ---- ARITHMETIC ----

    total++;
    if (run_test("integer addition",
        "let x = 1 + 2\n",
        true)) passed++;

    total++;
    if (run_test("string concatenation",
        "let x = \"hello\" + \" world\"\n",
        true)) passed++;

    total++;
    if (run_test("type error in addition",
        "let x = 1 + \"two\"\n",
        false)) passed++;

    total++;
    if (run_test("float arithmetic",
        "let x = 1.5 + 2.5\n",
        true)) passed++;

    // ---- RBT (Runtime Bounded Types) ----

    total++;
    if (run_test("RBT prove on known integer (elide)",
        "let x = 42\n"
        "let y = !Integer x\n",
        true)) passed++;

    total++;
    if (run_test("RBT prove on known string (elide)",
        "let x = \"hello\"\n"
        "let y = !String x\n",
        true)) passed++;

    total++;
    if (run_test("RBT prove on disjoint type (error)",
        "let x = \"hello\"\n"
        "let y = !Integer x\n",
        false)) passed++;

    total++;
    if (run_test("RBT mask on disjoint type (runtime check)",
        "let x = \"hello\"\n"
        "let y = !~Integer x\n",
        true)) passed++;

    total++;
    if (run_test("RBT query",
        "let x = 42\n"
        "let y = ?Integer x\n",
        true)) passed++;

    // ---- LIST ----

    total++;
    if (run_test("list indexing",
        "let items = [1, 2, 3]\n"
        "let first = items[0]\n",
        true)) passed++;

    total++;
    if (run_test("list with type annotation",
        "let items :: List<Integer> = [1, 2, 3]\n",
        true)) passed++;

    // ---- FUNCTIONS ----

    total++;
    if (run_test("fn definition",
        "fn add(a :: Integer, b :: Integer) :: Integer {\n"
        "  a + b\n"
        "}\n",
        true)) passed++;

    total++;
    if (run_test("fn call",
        "fn greet(name :: String) {\n"
        "  output(name)\n"
        "}\n"
        "greet(\"world\")\n",
        true)) passed++;

    total++;
    if (run_test("fn wrong arg type",
        "fn f(x :: Integer) { x }\n"
        "f(\"hello\")\n",
        false)) passed++;

    total++;
    if (run_test("fn recursion",
        "fn fact(n :: Integer) :: Integer {\n"
        "  if n == 0 { 1 }\n"
        "  else { n * fact(n - 1) }\n"
        "}\n",
        true)) passed++;

    // ---- LD (TCO LAMBDA) ----

    total++;
    if (run_test("ld definition",
        "ld fib(n, a = 0, b = 1) -> {\n"
        "  if n == 0 { a }\n"
        "  else if n == 1 { b }\n"
        "  else { fib(n - 1, b, a + b) }\n"
        "}\n",
        true)) passed++;

    // ---- CONTROL FLOW ----

    total++;
    if (run_test("if statement",
        "let x = 42\n"
        "if x > 0 {\n"
        "  output(x)\n"
        "}\n",
        true)) passed++;

    total++;
    if (run_test("if condition must be bool",
        "if 42 {\n"
        "  output(1)\n"
        "}\n",
        false)) passed++;

    total++;
    if (run_test("for loop",
        "for i in 1..=10 {\n"
        "  output(i)\n"
        "}\n",
        true)) passed++;

    total++;
    if (run_test("break outside loop",
        "break\n",
        false)) passed++;

    total++;
    if (run_test("return outside function",
        "return 42\n",
        false)) passed++;

    // ---- MATCH ----

    total++;
    if (run_test("match expression",
        "let x = 1\n"
        "let y = match x {\n"
        "  1 => \"one\",\n"
        "  2 => \"two\",\n"
        "  _ => \"other\"\n"
        "}\n",
        true)) passed++;

    // ---- FLOW-SENSITIVE RBT ----

    total++;
    if (run_test("flow-sensitive RBT: if ?Integer x { !Integer x }",
        "let x :: Unknown = 42\n"
        "if ?Integer x {\n"
        "  let y = !Integer x\n"
        "}\n",
        true)) passed++;

    total++;
    if (run_test("flow-sensitive RBT: else branch negated",
        "let x :: Unknown = 42\n"
        "if ?Integer x {\n"
        "  let y = !Integer x\n"
        "} else {\n"
        "  let y = !String x\n"
        "}\n",
        false)) passed++;

    // ---- RBT WITH LIST<UNKNOWN> ----

    total++;
    if (run_test("RBT on List<Unknown> element",
        "let rbt :: List<Unknown> = [1, \"Hello\"]\n"
        "let proved = !String rbt[1]\n",
        true)) passed++;

    total++;
    if (run_test("RBT fail on List<Unknown> element",
        "let rbt :: List<Unknown> = [1, \"Hello\"]\n"
        "let cant = !Integer rbt[1]\n",
        false)) passed++;

    total++;
    if (run_test("RBT mask on List<Unknown>",
        "let numsOrStrings :: List<Unknown> = [1, \"Hi\", 23]\n"
        "let cantProveInteger = !~Integer numsOrStrings[1]\n",
        true)) passed++;

    // ---- IMPORT ----

    total++;
    if (run_test("import statement",
        "import { serve } from \"std/net\"\n",
        true)) passed++;

    // ---- SUMMARY ----

    std::cout << "\n===========================\n";
    std::cout << "Results: " << passed << "/" << total << " passed\n";

    if (passed != total) {
        std::cout << "\nFailed tests:\n";
        for (const auto& r : results) {
            if (!r.passed) {
                std::cout << "  - " << r.name;
                if (!r.error_msg.empty()) std::cout << " (" << r.error_msg << ")";
                std::cout << "\n";
            }
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "=== BangScript Semantic Analyzer Tests ===\n\n";
    run_all_tests();
    return 0;
}
