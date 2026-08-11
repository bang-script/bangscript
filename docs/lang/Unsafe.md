# Unsafe (C++/C interop)

```ts
unsafe {
  let ptr = malloc(1024)
  memcpy(ptr, source, 1024)
  free(ptr)
}

// FFI
extern fn printf(fmt :: *String, ..) :: Integer

unsafe {
  import std.core;
  std::cout << "Hello from C++" << 42 << std::endl;
}
```
