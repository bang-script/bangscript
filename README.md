# BangScript

![BangScript Logo](!.png)

BangScript is a gradually-typed compiled language. It features Runtime Bounded Typing (RBT), a novel approach to type safety that bridges static and dynamic typing through explicit, atomic runtime type assertions.

> [NOTE] This repository contains the source code for the `bsc` compiler. 

## What is BangScript?

BangScript is designed for developers who want the safety of static types without the ceremony of traditional type systems. It draws syntax from Lua, TypeScript, and C, but reimagines how type checking works at runtime.

The language is expression-oriented: `if`, `match`, `for`, and `while` all return values. Functions are declared with `fn` for named procedures and `ld` for lambdas. Variables use `let` for mutability and `const` for immutability. Type annotations use the `::` postfix operator.


## Runtime Bounded Typing

RBT is BangScript's core type system feature. It provides three operators for runtime type assertions:

| Operator | Behavior | Returns |
|----------|----------|---------|
| `!T expr` | Prove: checks type at runtime, fatal error if wrong | `T` |
| `!~T expr` | Mask: checks type at runtime, returns `nil` if wrong | `T \| nil` |
| `?T expr` | Query: checks type at runtime, returns boolean | `Bool` |

RBT operates on the `Unknown` type, which represents values whose type cannot be statically determined. This enables safe interaction with untyped data sources while maintaining explicit, searchable type contracts in code.

The compiler elides RBT checks when types are already statically proven, making them zero-cost in typed contexts.

```typescript
let mixed :: List<Unknown> = [1, "Hello"]

let proved = !String mixed[1]
// proved = "Hello"

let cantProve = !Integer mixed[1]
// RBT Error: Cannot prove that "mixed[1]" is of type "Integer"

let masked = !~Integer mixed[1]
// masked = nil
```

## Design Decisions

| Feature | Choice |
|---------|--------|
| Indexing | 0-based |
| Semicolons | Optional (newline terminates) |
| Tail calls | Guaranteed optimization for `ld` |
| Memory | GC by default, `unsafe` blocks for C interop |
| Object orientation | None — structs and functions only |

## Status

BangScript is in early development.

## License

MIT
