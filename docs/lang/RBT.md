# RBT!

```ts
// The core innovation of BangScript.
// RBT bridges static and dynamic typing with explicit,
// atomic runtime type assertions.

let mixed :: List<Unknown> = [1, "hello", 3.14]

// !T expr    - PROVE: runtime type check, fatal error if wrong
let n = !Integer mixed[0]      // n :: Integer, guaranteed
let s = !String mixed[1]       // s :: String, guaranteed
let bad = !Integer mixed[1]    // RBT Error: cannot prove String is Integer

// !~T expr   - MASK: runtime type check, returns nil if wrong
let maybe = !~Integer mixed[1]  // maybe :: Integer | nil (= nil)

// ?T expr    - QUERY: boolean check, no error
let isNum = ?Integer mixed[0]   // true
let isStr = ?Integer mixed[1]   // false

// Deep checking
let nested = [[1, 2], [3, 4]]
!List<Integer> nested[0]        // shallow: is it a list?
!!List<Integer> nested[0]       // deep: every element is Integer

// Compile-time elision: compiler skips ! when already proven
let x :: Integer = 5
let y = !Integer x              // Compiler: "Redundant proof, eliding"
```
