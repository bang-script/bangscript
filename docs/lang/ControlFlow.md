# Control Flow (EE)

Everything is an Expression

```lua
if cond {
  ..
} else if cond2 {
  ..
} else {
  ..
}

// Without `else`, returns nil if false branch
let maybe = if x > 0 {x} // :: Integer | nil
```

# Pattern matching

```ts
match value {
  0 => "zero",
  n if n > 0 => "positive",
  _ => "negative"
}

match point {
  {x:0,y:0} = "origin",
  {x:0, ..} = "on y axis",
  {.., y:0} = "on x axis",
  {x, y} = "at ({x}, {y})"
}
```

# While

```ts
while condition {
  // body
}

// break with value
let found = while true {
  let item = next()
  if item > 10 {
    break item
  }
}
```

# For-Loops

```ts
for i in 0..10 {
  output(i)   // 0 to 9
}

// Range (inclusive end)
for i in 0..=10 {
  output(i)   // 0 to 10
}

// Iterate collection
for item in list {
  output(item)
}

// Iterate with index
for i, item in enumerate(list) {
  output("{i}: {item}")
}

// Iterate map
for key, value in map {
  output("{key}: {value}")
}

// break with value
let found = for item in list {
  if item > 10 {
    break item
  }
}
```
