# Result<T, E>

```ts
// ---- Result<T, E> ----
fn divide(a :: Float, b :: Float) :: Result<Float, String> {
  if b == 0 {
    Error("Division by zero")
  } else {
    Ok(a / b)
  }
}

// Unwrap with RBT
let q = !Ok divide(10, 2)      // 5.0
let bad = !Ok divide(10, 0)    // Runtime error

// Safe unwrap
let maybe = !~Ok divide(10, 0)  // nil

// Propagation with ?
fn safeDivide(a, b, c) :: Result<Float, String> {
  let x = ?divide(a, b)    // if Error, return Error immediately
  let y = ?divide(x, c)
  Ok(y)
}

// Pattern match
match divide(10, 0) {
  Ok(v) => output(v),
  Error(msg) => output("Error: {msg}"),
}

// ---- try / catch ----
let result = try {
  riskyOperation()
} catch e :: NetworkError {
  output("Network failed: {e.message}")
  fallbackValue
} catch e {
  output("Unknown: {e}")
  nil
}
```
