# Variable declaration

In BangScript, variables are declared with either:

```ts
let // mutable
const // immutable

/*
Example
*/

let name = "BangScript"
const _pi = 3.14
```

# Types

Types are anottated via `::`

Example:
```ts
let name :: String = "BangScript!"
const _pi :: Float = 3.14159265
```

Reassigning a variable of type A to a value of type B gives an error:

```
// Error: Variable 'x' of type 'X' cannot be reassigned to: {value: "y", type: "z"}
```

| Type | Example |
| :--- | ---: |
| Integer | 2763 |
| Float | 3.14 |
| String | "Hello!" |
| Bool | true or false |
| nil | absence |
| List<T> | ordered sequence |
| Map<K, V> | key-value pairs |
| Set<V> | unique values |
| Unknown | RBT-type, used when type is not known |

# Type Unions:

```ts
type Number = Integer | Float
type Maybe<T> = T | nil
```
