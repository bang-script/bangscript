# Structs: `type`

They are generic structs:

```ts
type Box<T> = {
  value :: T,
  map: fn(fn(T): U): Box<U>,
}

type Animal = {
  name :: String,
  breed :: String,
}
```

# Recursive types, such as Trees

```ts
type Tree<T> = {
  value :: T,
  left :: Tree<T> | nil,
  right :: Tree<T> | nil,
}
```
