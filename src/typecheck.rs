// ============================================================================
// BangScript Type Checker
// ============================================================================

use crate::ast::*;
use std::collections::HashMap;

// ============================================================================
// Type Variable for Unification
// ============================================================================

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum TypeVar {
    Bound(Type),
    Unbound(u32), // unique id
}

static mut NEXT_VAR: u32 = 0;

fn fresh_var() -> Type {
    unsafe {
        let v = NEXT_VAR;
        NEXT_VAR += 1;
        Type::Var(v)
    }
}

// ============================================================================
// Type (extended with variables for unification)
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
    Var(u32), // type variable for unification
}

// ============================================================================
// Substitution / Unification
// ============================================================================

#[derive(Debug, Clone)]
pub struct Substitution {
    bindings: HashMap<u32, Type>,
}

impl Substitution {
    pub fn new() -> Self {
        Self { bindings: HashMap::new() }
    }

    pub fn bind(&mut self, var: u32, typ: Type) {
        // Occurs check
        if self.occurs_in(var, &typ) {
            return; // cycle, will report as error
        }
        self.bindings.insert(var, typ);
    }

    fn occurs_in(&self, var: u32, typ: &Type) -> bool {
        match typ {
            Type::Var(v) => *v == var || self.bindings.get(v).map_or(false, |t| self.occurs_in(var, t)),
            Type::List(t) => self.occurs_in(var, t),
            Type::Map(k, v) => self.occurs_in(var, k) || self.occurs_in(var, v),
            Type::Set(t) => self.occurs_in(var, t),
            Type::Function(params, ret) => {
                params.iter().any(|p| self.occurs_in(var, p)) || self.occurs_in(var, ret)
            }
            Type::Union(a, b) => self.occurs_in(var, a) || self.occurs_in(var, b),
            Type::Result(a, b) => self.occurs_in(var, a) || self.occurs_in(var, b),
            Type::Maybe(t) => self.occurs_in(var, t),
            _ => false,
        }
    }

    pub fn apply(&self, typ: &Type) -> Type {
        match typ {
            Type::Var(v) => {
                if let Some(bound) = self.bindings.get(v) {
                    self.apply(bound)
                } else {
                    Type::Var(*v)
                }
            }
            Type::List(t) => Type::List(Box::new(self.apply(t))),
            Type::Map(k, v) => Type::Map(Box::new(self.apply(k)), Box::new(self.apply(v))),
            Type::Set(t) => Type::Set(Box::new(self.apply(t))),
            Type::Function(params, ret) => {
                let new_params = params.iter().map(|p| self.apply(p)).collect();
                Type::Function(new_params, Box::new(self.apply(ret)))
            }
            Type::Union(a, b) => Type::Union(Box::new(self.apply(a)), Box::new(self.apply(b))),
            Type::Result(a, b) => Type::Result(Box::new(self.apply(a)), Box::new(self.apply(b))),
            Type::Maybe(t) => Type::Maybe(Box::new(self.apply(t))),
            other => other.clone(),
        }
    }

    pub fn unify(&mut self, a: &Type, b: &Type) -> Result<(), String> {
        let a = self.apply(a);
        let b = self.apply(b);

        if a == b {
            return Ok(());
        }

        match (&a, &b) {
            (Type::Var(v), _) => {
                if self.occurs_in(*v, &b) {
                    return Err(format!("Occurs check failed: {:?} in {:?}", v, b));
                }
                self.bind(*v, b.clone());
                Ok(())
            }
            (_, Type::Var(v)) => {
                if self.occurs_in(*v, &a) {
                    return Err(format!("Occurs check failed: {:?} in {:?}", v, a));
                }
                self.bind(*v, a.clone());
                Ok(())
            }
            (Type::List(t1), Type::List(t2)) => self.unify(t1, t2),
            (Type::Map(k1, v1), Type::Map(k2, v2)) => {
                self.unify(k1, k2)?;
                self.unify(v1, v2)
            }
            (Type::Set(t1), Type::Set(t2)) => self.unify(t1, t2),
            (Type::Function(p1, r1), Type::Function(p2, r2)) => {
                if p1.len() != p2.len() {
                    return Err(format!("Function arity mismatch: {} vs {}", p1.len(), p2.len()));
                }
                for (pa, pb) in p1.iter().zip(p2.iter()) {
                    self.unify(pa, pb)?;
                }
                self.unify(r1, r2)
            }
            (Type::Union(a1, b1), Type::Union(a2, b2)) => {
                self.unify(a1, a2)?;
                self.unify(b1, b2)
            }
            (Type::Result(a1, b1), Type::Result(a2, b2)) => {
                self.unify(a1, a2)?;
                self.unify(b1, b2)
            }
            (Type::Maybe(t1), Type::Maybe(t2)) => self.unify(t1, t2),
            // Subtyping: Integer <: Float
            (Type::Integer, Type::Float) => Ok(()),
            (Type::Named(n1), Type::Named(n2)) if n1 == n2 => Ok(()),
            _ => Err(format!("Cannot unify {:?} with {:?}", a, b)),
        }
    }
}

// ============================================================================
// Type Environment with Struct Fields
// ============================================================================

#[derive(Debug, Clone)]
pub struct StructInfo {
    pub fields: HashMap<String, Type>,
}

#[derive(Debug, Clone)]
pub struct TypeEnv {
    vars: HashMap<String, Type>,
    types: HashMap<String, Type>,        // type aliases
    structs: HashMap<String, StructInfo>, // struct definitions
    parent: Option<Box<TypeEnv>>,
}

impl TypeEnv {
    pub fn new() -> Self {
        Self {
            vars: HashMap::new(),
            types: HashMap::new(),
            structs: HashMap::new(),
            parent: None,
        }
    }

    pub fn child(parent: &TypeEnv) -> Self {
        Self {
            vars: HashMap::new(),
            types: HashMap::new(),
            structs: HashMap::new(),
            parent: Some(Box::new(parent.clone())),
        }
    }

    pub fn define_var(&mut self, name: &str, typ: Type) {
        self.vars.insert(name.to_string(), typ);
    }

    pub fn define_type(&mut self, name: &str, typ: Type) {
        self.types.insert(name.to_string(), typ);
    }

    pub fn define_struct(&mut self, name: &str, info: StructInfo) {
        self.structs.insert(name.to_string(), info);
    }

    pub fn lookup_var(&self, name: &str) -> Option<Type> {
        self.vars.get(name).cloned()
            .or_else(|| self.parent.as_ref().and_then(|p| p.lookup_var(name)))
    }

    pub fn lookup_type(&self, name: &str) -> Option<Type> {
        self.types.get(name).cloned()
            .or_else(|| self.parent.as_ref().and_then(|p| p.lookup_type(name)))
    }

    pub fn lookup_struct(&self, name: &str) -> Option<StructInfo> {
        self.structs.get(name).cloned()
            .or_else(|| self.parent.as_ref().and_then(|p| p.lookup_struct(name)))
    }

    pub fn resolve_type(&self, typ: &Type) -> Type {
        match typ {
            Type::Named(name) => {
                self.lookup_type(name).unwrap_or_else(|| Type::Named(name.clone()))
            }
            _ => typ.clone(),
        }
    }
}

// ============================================================================
// Return Type Tracking
// ============================================================================

#[derive(Debug, Clone)]
pub struct ReturnInfo {
    pub expected: Option<Type>,
    pub found: Vec<(Type, Loc)>,
}

// ============================================================================
// Type Checker
// ============================================================================

use crate::lexer::Loc;

pub struct TypeChecker {
    env: TypeEnv,
    sub: Substitution,
    errors: Vec<String>,
    return_info: Option<ReturnInfo>,
}

impl TypeChecker {
    pub fn new() -> Self {
        let mut env = TypeEnv::new();
        env.define_var("output", Type::Function(vec![Type::Unknown], Box::new(Type::Nil)));
        env.define_var("input", Type::Function(vec![], Box::new(Type::String)));
        env.define_var("print", Type::Function(vec![Type::Unknown], Box::new(Type::Nil)));
        env.define_var("typeof", Type::Function(vec![Type::Unknown], Box::new(Type::String)));
        env.define_var("assert", Type::Function(vec![Type::Bool], Box::new(Type::Nil)));
        Self {
            env,
            sub: Substitution::new(),
            errors: Vec::new(),
            return_info: None,
        }
    }

    fn error(&mut self, msg: &str) {
        self.errors.push(msg.to_string());
    }

    fn apply(&self, typ: &Type) -> Type {
        self.sub.apply(typ)
    }

    fn unify(&mut self, a: &Type, b: &Type) -> Result<(), ()> {
        if let Err(e) = self.sub.unify(a, b) {
            self.error(&e);
            Err(())
        } else {
            Ok(())
        }
    }

    // ------------------------------------------------------------------------
    // Expression type inference
    // ------------------------------------------------------------------------

    pub fn infer_expr(&mut self, expr: &Expr) -> Type {
        match expr {
            Expr::Integer(_) => Type::Integer,
            Expr::Float(_) => Type::Float,
            Expr::String(_) | Expr::RawString(_) => Type::String,
            Expr::Bool(_) => Type::Bool,
            Expr::Nil => Type::Nil,
            Expr::Ident(name) => {
                self.env.lookup_var(name).unwrap_or_else(|| {
                    self.error(&format!("Undefined variable: {}", name));
                    Type::Unknown
                })
            }
            Expr::Binary { op, left, right } => {
                let lt = self.infer_expr(left);
                let rt = self.infer_expr(right);
                match op {
                    BinOp::Add | BinOp::Sub | BinOp::Mul | BinOp::Div | BinOp::Mod => {
                        let result = fresh_var();
                        let _ = self.unify(&lt, &Type::Var(0)); // number constraint
                        let _ = self.unify(&rt, &Type::Var(0));
                        if lt == Type::String || rt == Type::String {
                            Type::String
                        } else {
                            let _ = self.unify(&result, &lt);
                            self.apply(&result)
                        }
                    }
                    BinOp::Eq | BinOp::Neq => {
                        let _ = self.unify(&lt, &rt);
                        Type::Bool
                    }
                    BinOp::Lt | BinOp::Gt | BinOp::Leq | BinOp::Geq => {
                        let _ = self.unify(&lt, &rt);
                        Type::Bool
                    }
                    BinOp::And | BinOp::Or => {
                        let _ = self.unify(&lt, &Type::Bool);
                        let _ = self.unify(&rt, &Type::Bool);
                        Type::Bool
                    }
                }
            }
            Expr::Unary { op, expr } => {
                let t = self.infer_expr(expr);
                match op {
                    UnOp::Neg => {
                        let _ = self.unify(&t, &Type::Var(0));
                        self.apply(&t)
                    }
                    UnOp::Not => {
                        let _ = self.unify(&t, &Type::Bool);
                        Type::Bool
                    }
                }
            }
            Expr::Assign { target, value } => {
                let tt = match target.as_ref() {
                    Expr::Ident(name) => self.env.lookup_var(name).unwrap_or(Type::Unknown),
                    _ => Type::Unknown,
                };
                let vt = self.infer_expr(value);
                let _ = self.unify(&tt, &vt);
                self.apply(&vt)
            }
            Expr::Call { func, args } => {
                let ft = self.infer_expr(func);
                let ft = self.apply(&ft);
                match ft {
                    Type::Function(param_types, ret) => {
                        if args.len() != param_types.len() {
                            self.error(&format!("Arg count: expected {}, got {}", param_types.len(), args.len()));
                        }
                        for (arg, param) in args.iter().zip(param_types.iter()) {
                            let at = self.infer_expr(arg);
                            let _ = self.unify(&at, param);
                        }
                        self.apply(&ret)
                    }
                    Type::Unknown => Type::Unknown,
                    _ => {
                        self.error(&format!("Not a function: {:?}", ft));
                        Type::Unknown
                    }
                }
            }
            Expr::List(items) => {
                if items.is_empty() {
                    Type::List(Box::new(fresh_var()))
                } else {
                    let elem_type = self.infer_expr(&items[0]);
                    for item in &items[1..] {
                        let it = self.infer_expr(item);
                        let _ = self.unify(&elem_type, &it);
                    }
                    Type::List(Box::new(self.apply(&elem_type)))
                }
            }
            Expr::Map(items) => {
                if items.is_empty() {
                    Type::Map(Box::new(Type::String), Box::new(fresh_var()))
                } else {
                    let value_type = self.infer_expr(&items[0].1);
                    for (_, val) in &items[1..] {
                        let vt = self.infer_expr(val);
                        let _ = self.unify(&value_type, &vt);
                    }
                    Type::Map(Box::new(Type::String), Box::new(self.apply(&value_type)))
                }
            }
            Expr::Range { start, end, .. } => {
                let st = self.infer_expr(start);
                let et = self.infer_expr(end);
                let _ = self.unify(&st, &et);
                Type::Named("Range".to_string())
            }
            Expr::Index { expr, index } => {
                let et = self.infer_expr(expr);
                let it = self.infer_expr(index);
                let _ = self.unify(&it, &Type::Integer);
                match self.apply(&et) {
                    Type::List(t) => *t,
                    Type::Map(_, v) => *v,
                    Type::Unknown => Type::Unknown,
                    _ => {
                        self.error(&format!("Cannot index {:?}", et));
                        Type::Unknown
                    }
                }
            }
            Expr::Member { expr, name } => {
                let et = self.infer_expr(expr);
                let et = self.apply(&et);
                match &et {
                    Type::Named(struct_name) => {
                        if let Some(info) = self.env.lookup_struct(struct_name) {
                            info.fields.get(name).cloned().unwrap_or_else(|| {
                                self.error(&format!("No field '{}' on struct {}", name, struct_name));
                                Type::Unknown
                            })
                        } else {
                            self.error(&format!("Unknown struct: {}", struct_name));
                            Type::Unknown
                        }
                    }
                    Type::Map(_, v) => *v.clone(),
                    Type::Unknown => Type::Unknown,
                    _ => {
                        self.error(&format!("No member '{}' on {:?}", name, et));
                        Type::Unknown
                    }
                }
            }
            Expr::If { cond, then_branch, else_branch } => {
                let ct = self.infer_expr(cond);
                let _ = self.unify(&ct, &Type::Bool);

                let mut then_type = Type::Nil;
                for stmt in then_branch {
                    then_type = self.check_stmt(stmt);
                }
                let then_type = self.apply(&then_type);

                if let Some(else_stmts) = else_branch {
                    let mut else_type = Type::Nil;
                    for stmt in else_stmts {
                        else_type = self.check_stmt(stmt);
                    }
                    let else_type = self.apply(&else_type);

                    if then_type != else_type && then_type != Type::Unknown && else_type != Type::Unknown {
                        Type::Union(Box::new(then_type), Box::new(else_type))
                    } else {
                        then_type
                    }
                } else {
                    Type::Union(Box::new(then_type), Box::new(Type::Nil))
                }
            }
            Expr::Match { expr, arms } => {
                let mt = self.infer_expr(expr);
                let mut result_type = Type::Nil;
                let mut first = true;

                for (pat, body) in arms {
                    let pat_type = self.infer_pattern(pat);
                    let _ = self.unify(&mt, &pat_type);

                    let mut arm_type = Type::Nil;
                    for stmt in body {
                        arm_type = self.check_stmt(stmt);
                    }
                    let arm_type = self.apply(&arm_type);

                    if first {
                        result_type = arm_type;
                        first = false;
                    } else {
                        let _ = self.unify(&result_type, &arm_type);
                    }
                }

                self.apply(&result_type)
            }
            Expr::Lambda { params, body, .. } => {
                let param_types: Vec<Type> = params.iter().map(|(_, t)| {
                    t.clone().unwrap_or_else(fresh_var)
                }).collect();
                let mut body_type = Type::Nil;

                let mut child = TypeEnv::child(&self.env);
                for ((name, _), ptyp) in params.iter().zip(param_types.iter()) {
                    child.define_var(name, ptyp.clone());
                }

                let old_env = std::mem::replace(&mut self.env, child);
                let old_return = self.return_info.take();

                for stmt in body {
                    body_type = self.check_stmt(stmt);
                }

                self.return_info = old_return;
                self.env = old_env;

                Type::Function(param_types, Box::new(self.apply(&body_type)))
            }
            Expr::FnExpr { params, ret_type, body } => {
                let param_types: Vec<Type> = params.iter().map(|(_, t)| {
                    t.clone().unwrap_or_else(fresh_var)
                }).collect();
                let expected_ret = ret_type.clone().unwrap_or_else(fresh_var);

                let mut child = TypeEnv::child(&self.env);
                for ((name, _), ptyp) in params.iter().zip(param_types.iter()) {
                    child.define_var(name, ptyp.clone());
                }

                let old_env = std::mem::replace(&mut self.env, child);
                let old_return = std::mem::replace(&mut self.return_info, Some(ReturnInfo {
                    expected: Some(expected_ret.clone()),
                    found: Vec::new(),
                }));

                let mut body_type = Type::Nil;
                for stmt in body {
                    body_type = self.check_stmt(stmt);
                }

                // Check all return types
                if let Some(ref info) = self.return_info {
                    if let Some(ref expected) = info.expected {
                        for (found, _) in &info.found {
                            let _ = self.unify(expected, found);
                        }
                    }
                }

                self.return_info = old_return;
                self.env = old_env;

                Type::Function(param_types, Box::new(self.apply(&expected_ret)))
            }
            Expr::RbtProve { typ, expr } => {
                let et = self.infer_expr(expr);
                let resolved = self.env.resolve_type(typ);
                let _ = self.unify(&resolved, &et);
                self.apply(&resolved)
            }
            Expr::RbtMask { typ, expr } => {
                let et = self.infer_expr(expr);
                let resolved = self.env.resolve_type(typ);
                let _ = self.unify(&resolved, &et);
                Type::Maybe(Box::new(self.apply(&resolved)))
            }
            Expr::RbtQuery { typ, expr } => {
                let _et = self.infer_expr(expr);
                let _resolved = self.env.resolve_type(typ);
                Type::Bool
            }
        }
    }

    // ------------------------------------------------------------------------
    // Pattern type inference
    // ------------------------------------------------------------------------

    fn infer_pattern(&mut self, pat: &Pattern) -> Type {
        match pat {
            Pattern::Wildcard => fresh_var(),
            Pattern::Literal(expr) => self.infer_expr(expr),
            Pattern::Variable(_) => fresh_var(),
            Pattern::Guarded { pattern, .. } => self.infer_pattern(pattern),
            Pattern::Struct(fields) => {
                let mut field_types = HashMap::new();
                for (name, pat) in fields {
                    field_types.insert(name.clone(), self.infer_pattern(pat));
                }
                // Return as a map type for now
                Type::Map(Box::new(Type::String), Box::new(fresh_var()))
            }
            Pattern::List(items) => {
                if items.is_empty() {
                    Type::List(Box::new(fresh_var()))
                } else {
                    let elem_type = self.infer_pattern(&items[0]);
                    Type::List(Box::new(elem_type))
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // Statement type checking
    // ------------------------------------------------------------------------

    pub fn check_stmt(&mut self, stmt: &Stmt) -> Type {
        match stmt {
            Stmt::Let { name, typ, mutable: _, value } => {
                let vt = self.infer_expr(value);
                let final_type = if let Some(annotated) = typ {
                    let resolved = self.env.resolve_type(annotated);
                    let _ = self.unify(&resolved, &vt);
                    self.apply(&resolved)
                } else {
                    self.apply(&vt)
                };
                self.env.define_var(name, final_type.clone());
                final_type
            }
            Stmt::Const { name, typ, value } => {
                let vt = self.infer_expr(value);
                let final_type = if let Some(annotated) = typ {
                    let resolved = self.env.resolve_type(annotated);
                    let _ = self.unify(&resolved, &vt);
                    self.apply(&resolved)
                } else {
                    self.apply(&vt)
                };
                self.env.define_var(name, final_type.clone());
                final_type
            }
            Stmt::Assign { target, value } => {
                let tt = match target {
                    Expr::Ident(name) => self.env.lookup_var(name).unwrap_or(Type::Unknown),
                    _ => Type::Unknown,
                };
                let vt = self.infer_expr(value);
                let _ = self.unify(&tt, &vt);
                self.apply(&vt)
            }
            Stmt::Expr(expr) => self.infer_expr(expr),
            Stmt::Return(expr) => {
                let ret_type = if let Some(e) = expr {
                    self.infer_expr(e)
                } else {
                    Type::Nil
                };

                if let Some(ref mut info) = self.return_info {
                    info.found.push((ret_type.clone(), Loc::new(0, 0, 0, 0)));
                }

                ret_type
            }
            Stmt::Break | Stmt::Continue => Type::Nil,
            Stmt::FnDecl { name, params, ret_type, body } => {
                let param_types: Vec<Type> = params.iter().map(|(_, t)| {
                    t.clone().unwrap_or_else(fresh_var)
                }).collect();
                let expected_ret = ret_type.clone().unwrap_or_else(fresh_var);

                // Define function in parent scope before checking body
                let func_type = Type::Function(param_types.clone(), Box::new(expected_ret.clone()));
                self.env.define_var(name, func_type);

                let mut child = TypeEnv::child(&self.env);
                for ((pname, _), ptyp) in params.iter().zip(param_types.iter()) {
                    child.define_var(pname, ptyp.clone());
                }

                let old_env = std::mem::replace(&mut self.env, child);
                let old_return = std::mem::replace(&mut self.return_info, Some(ReturnInfo {
                    expected: Some(expected_ret.clone()),
                    found: Vec::new(),
                }));

                let mut body_type = Type::Nil;
                for stmt in body {
                    body_type = self.check_stmt(stmt);
                }

                // Check all return types match expected
                if let Some(ref info) = self.return_info {
                    if let Some(ref expected) = info.expected {
                        for (found, _) in &info.found {
                            let _ = self.unify(expected, found);
                        }
                    }
                }

                self.return_info = old_return;
                self.env = old_env;

                self.apply(&expected_ret)
            }
            Stmt::LdDecl { name, params, body, .. } => {
                let param_types: Vec<Type> = params.iter().map(|(_, t)| {
                    t.clone().unwrap_or_else(fresh_var)
                }).collect();

                let func_type = Type::Function(param_types.clone(), Box::new(fresh_var()));
                self.env.define_var(name, func_type);

                let mut child = TypeEnv::child(&self.env);
                for ((pname, _), ptyp) in params.iter().zip(param_types.iter()) {
                    child.define_var(pname, ptyp.clone());
                }

                let old_env = std::mem::replace(&mut self.env, child);
                let old_return = self.return_info.take();

                let mut body_type = Type::Nil;
                for stmt in body {
                    body_type = self.check_stmt(stmt);
                }

                self.return_info = old_return;
                self.env = old_env;

                self.apply(&body_type)
            }
            Stmt::TypeDecl { name, typ } => {
                let resolved = self.env.resolve_type(typ);

                // If it's a struct type (map), extract fields
                if let Type::Map(_, _) = &resolved {
                    let mut fields = HashMap::new();
                    // TODO: extract fields from map literal in type declaration
                    self.env.define_struct(name, StructInfo { fields });
                }

                self.env.define_type(name, resolved.clone());
                resolved
            }
            Stmt::Import { .. } => Type::Nil,
            Stmt::Export(stmt) => self.check_stmt(stmt),
            Stmt::Block(stmts) => {
                let mut last = Type::Nil;
                for stmt in stmts {
                    last = self.check_stmt(stmt);
                }
                last
            }
            Stmt::While { cond, body } => {
                let ct = self.infer_expr(cond);
                let _ = self.unify(&ct, &Type::Bool);
                for stmt in body { self.check_stmt(stmt); }
                Type::Nil
            }
            Stmt::For { var, iterable, body } => {
                let it = self.infer_expr(iterable);
                let elem_type = match self.apply(&it) {
                    Type::List(t) => *t,
                    Type::Map(_, v) => *v,
                    Type::Unknown => Type::Unknown,
                    _ => { self.error(&format!("Cannot iterate {:?}", it)); Type::Unknown }
                };

                let mut child = TypeEnv::child(&self.env);
                child.define_var(var, elem_type);
                let old_env = std::mem::replace(&mut self.env, child);
                for stmt in body { self.check_stmt(stmt); }
                self.env = old_env;
                Type::Nil
            }
            Stmt::Unsafe(body) => {
                let mut last = Type::Nil;
                for stmt in body { last = self.check_stmt(stmt); }
                last
            }
            Stmt::Spawn(expr) => {
                self.infer_expr(expr);
                Type::Nil
            }
            Stmt::Try { body, catches } => {
                for stmt in body { self.check_stmt(stmt); }
                for (_, error_type, catch_body) in catches {
                    let mut child = TypeEnv::child(&self.env);
                    if let Some(et) = error_type {
                        child.define_var("e", self.env.resolve_type(et));
                    }
                    let old_env = std::mem::replace(&mut self.env, child);
                    for stmt in catch_body { self.check_stmt(stmt); }
                    self.env = old_env;
                }
                Type::Nil
            }
        }
    }

    // ------------------------------------------------------------------------
    // Top-level check
    // ------------------------------------------------------------------------

    pub fn check(&mut self, stmts: &[Stmt]) -> Vec<String> {
        for stmt in stmts {
            self.check_stmt(stmt);
        }
        self.errors.clone()
    }
}
