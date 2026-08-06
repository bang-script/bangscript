// ============================================================================
// BangScript AST
// Abstract syntax tree definitions for the BangScript language.
// ============================================================================

// ============================================================================
// Types
// ============================================================================

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Type {
    Unknown,
    Integer,
    Float,
    String,
    Bool,
    Nil,
    List(Box<Type>),
    Map(Box<Type>, Box<Type>),
    Set(Box<Type>),
    Function(Vec<Type>, Box<Type>),
    Named(String),
    Union(Box<Type>, Box<Type>),
    Result(Box<Type>, Box<Type>),
    Maybe(Box<Type>),
}

// ============================================================================
// Binary Operators
// ============================================================================

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum BinOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Gt, Leq, Geq,
    And, Or,
}

// ============================================================================
// Unary Operators
// ============================================================================

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum UnOp {
    Neg, Not,
}

// ============================================================================
// Patterns
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    Wildcard,
    Literal(Expr),
    Variable(String),
    Guarded { pattern: Box<Pattern>, cond: Box<Expr> },
    Struct(Vec<(String, Pattern)>),
    List(Vec<Pattern>),
}

// ============================================================================
// Expressions
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Integer(i64),
    Float(f64),
    String(String),
    RawString(String),
    Bool(bool),
    Nil,
    Ident(String),

    // Binary operations
    Binary { op: BinOp, left: Box<Expr>, right: Box<Expr> },

    // Unary operations
    Unary { op: UnOp, expr: Box<Expr> },

    // Assignment (also an expression in BangScript)
    Assign { target: Box<Expr>, value: Box<Expr> },

    // Function call
    Call { func: Box<Expr>, args: Vec<Expr> },

    // List literal
    List(Vec<Expr>),

    // Map literal
    Map(Vec<(String, Expr)>),

    // Range
    Range { start: Box<Expr>, end: Box<Expr>, inclusive: bool },

    // Indexing
    Index { expr: Box<Expr>, index: Box<Expr> },

    // Member access
    Member { expr: Box<Expr>, name: String },

    // if expression
    If { cond: Box<Expr>, then_branch: Vec<Stmt>, else_branch: Option<Vec<Stmt>> },

    // match expression
    Match { expr: Box<Expr>, arms: Vec<(Pattern, Vec<Stmt>)> },

    // Lambda (ld)
    Lambda { name: Option<String>, params: Vec<(String, Option<Type>)>, defaults: Vec<(usize, Expr)>, body: Vec<Stmt> },

    // Anonymous function (fn expression)
    FnExpr { params: Vec<(String, Option<Type>)>, ret_type: Option<Type>, body: Vec<Stmt> },

    // RBT operators
    RbtProve { typ: Type, expr: Box<Expr> },
    RbtMask { typ: Type, expr: Box<Expr> },
    RbtQuery { typ: Type, expr: Box<Expr> },
}

// ============================================================================
// Statements
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    Let { name: String, typ: Option<Type>, mutable: bool, value: Expr },
    Const { name: String, typ: Option<Type>, value: Expr },
    Assign { target: Expr, value: Expr },
    Expr(Expr),
    Return(Option<Expr>),
    Break,
    Continue,

    FnDecl { name: String, params: Vec<(String, Option<Type>)>, ret_type: Option<Type>, body: Vec<Stmt> },
    LdDecl { name: String, params: Vec<(String, Option<Type>)>, defaults: Vec<(usize, Expr)>, body: Vec<Stmt> },
    TypeDecl { name: String, typ: Type },
    Import { items: Vec<String>, aliases: std::collections::HashMap<String, String>, source: String },
    Export(Box<Stmt>),
    Block(Vec<Stmt>),
    While { cond: Expr, body: Vec<Stmt> },
    For { var: String, iterable: Expr, body: Vec<Stmt> },
    Unsafe(Vec<Stmt>),
    Spawn(Box<Expr>),
    Try { body: Vec<Stmt>, catches: Vec<(String, Option<Type>, Vec<Stmt>)> },
}
