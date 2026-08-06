// ============================================================================
// BangScript Compiler — Main Entry Point
// ============================================================================

mod ast;
mod lexer;
mod parser;
mod typecheck;
mod codegen;

use std::env;
use std::fs;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("Usage: bsc <input.bs> [-o output.wasm]");
        process::exit(1);
    }

    let input_file = &args[1];
    let output_file = args.iter().position(|a| a == "-o")
        .and_then(|i| args.get(i + 1))
        .map(|s| s.clone())
        .unwrap_or_else(|| "output.wasm".to_string());

    let source = match fs::read_to_string(input_file) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Error reading {}: {}", input_file, e);
            process::exit(1);
        }
    };

    println!("=== BangScript Compiler ===");
    println!("Input:  {}", input_file);
    println!("Output: {}", output_file);
    println!();

    // Phase 1: Lexing
    println!("[1/4] Lexing...");
    let lexer = lexer::Lexer::new(&source);
    let tokens = match lexer.tokenize() {
        Ok(t) => t,
        Err(e) => {
            eprintln!("Lex error: {}", e);
            process::exit(1);
        }
    };
    println!("      {} tokens", tokens.len());

    // Phase 2: Parsing
    println!("[2/4] Parsing...");
    let mut parser = parser::Parser::new(tokens);
    let ast = match parser.parse() {
        Ok(a) => a,
        Err(e) => {
            eprintln!("Parse error: {}", e);
            process::exit(1);
        }
    };
    println!("      {} top-level statements", ast.len());

    // Phase 3: Type checking
    println!("[3/4] Type checking...");
    let mut checker = typecheck::TypeChecker::new();
    let errors = checker.check(&ast);
    if errors.is_empty() {
        println!("      No errors");
    } else {
        println!("      {} error(s) found:", errors.len());
        for err in &errors {
            println!("        ! {}", err);
        }
    }

    // Phase 4: Code generation
    println!("[4/4] Generating WASM...");
    let mut gen = codegen::CodeGen::new();
    let wasm_text = gen.compile(&ast);

    let wat_file = output_file.replace(".wasm", ".wat");
    match fs::write(&wat_file, &wasm_text) {
        Ok(_) => println!("      WAT written to {}", wat_file),
        Err(e) => {
            eprintln!("Error writing {}: {}", wat_file, e);
            process::exit(1);
        }
    }

    println!();
    println!("To assemble: wasm-tools parse {} -o {}", wat_file, output_file);
    println!("To run:      wasmtime {}", output_file);
    println!();
    println!("=== Done ===");
}
