# Modules

```ts
// ---- Import ----
import { map, filter, fold } from "std/list"
import math from "std/math"
import { json_parse as parseJson } from "std/json"

// Import all (discouraged)
import * as fs from "std/fs"

// ---- Export ----
export fn add(a, b) { a + b }
export fn sub(a, b) { a - b }
export const PI :: Float = 3.14159

export type Point = {
  x :: Float,
  y :: Float,
}

// Re-export
export { sin, cos, tan } from "std/trig"

// Module path resolution:
// "std/..."      - Standard library
// "./..."        - Relative to current file
// "../..."       - Parent directory
// "package/..."  - Third-party package
```
