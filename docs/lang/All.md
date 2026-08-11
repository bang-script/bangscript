# BangScript (all)

```ts

// ---- FizzBuzz ----
fn fizzbuzz(n :: Integer) {
  for i in 1..=n {
    let out = ""
    if i % 3 == 0 { out = out + "Fizz" }
    if i % 5 == 0 { out = out + "Buzz" }
    if out == "" { output(i) } else { output(out) }
  }
}

fizzbuzz(100)

// ---- Fibonacci (TCO) ----
ld fib(n, a = 0, b = 1) -> {
  if n == 0 { a }
  else if n == 1 { b }
  else { fib(n - 1, b, a + b) }
}

output(fib(1000))   // No stack overflow, TCO optimized

// ---- HTTP Server ----
import { serve } from "std/net"

let server = serve("localhost", 8080)

for request in server {
  match request.path {
    "/" => request.respond("Hello, BangScript!"),
    "/health" => request.respond('{"status": "ok"}'),
    _ => request.respond("Not found", status: 404),
  }
}

// ---- RBT Demo ----
let rbt :: List<Unknown> = [1, "Hello"]

let proved = !String rbt[1]
// proved = "Hello"

let cantProve = !Integer rbt[1]
/*
RBT Error:
- Cannot prove that "rbt[1]" is of type "Integer" in the case of "cantProve"
- Use "!~" for fallback (returns "nil")
*/

let numsOrStrings :: List<Unknown> = [1, "Hi", 23]
let cantProveInteger = !~Integer numsOrStrings[1]
// cantProveInteger = nil
/*
RBT Warning:
- Masked fatal error of case "cantProveInteger"
- Cause: "cantProveInteger" cannot prove that "numsOrStrings[1]" is of type "Integer"
*/
```
