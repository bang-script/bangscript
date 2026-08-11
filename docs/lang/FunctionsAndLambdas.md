# Functions

In BangScript, functions are defined with 
```rs
fn
```

Function declaration template:

```rs
fn name(param :: Type) :: ReturnType {
  return X
}
```

```rs
// For example:

fn double(x::Float)::Float {
  return x*2
}
```

# Lambdas

Lambdas are anonymous functions, declared with `ld`

```ts
ld double(x) -> x*2

ld factorial(n) -> {
  if n <= 1 { 1 }
  else {n*factorial(n-1)}
}

double(2)
// => 4
```
