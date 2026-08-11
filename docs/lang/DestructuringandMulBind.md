# Multiple Bindings

Like in Lua, multiple variables can all recieve values at the same time

let a, b, c = 1, 2, 3

# Destructuring

```ts
let {x, y} = point
let [first, second, ..rest] = list
let [x, y, ..] = coords
// ".." for "ignore rest"
```

