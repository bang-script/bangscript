# Macros (no parens)

```ts
// Macros operate on AST (represented as tables) at compile time

macro unless(cond, body) {
  quote {
    if !unquote(cond) {
      unquote(body)
    }
  }
}

// Usage
unless(x == 0) {
  output("x is not zero")
}

// Quasiquote with splicing
macro when(cond, ..body) {
  quasiquote {
    if unquote(cond) {
      unquote_splice(body)
    }
  }
}

when(x > 0) {
  output("positive")
  doSomething()
}

// Macros are hygienic by default
```
