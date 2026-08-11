# Channels

```ts
// Spawn lightweight threads
spawn fn() {
  output("Background task")
}

// Channels
let ch = channel(String)

spawn fn() {
  ch.send("Hello from task")
}

let msg = ch.receive()   // "Hello from task"

// Select (non-blocking multi-channel)
select {
  ch1.receive() => |msg| {
    output("ch1: {msg}")
  },
  ch2.receive() => |msg| {
    output("ch2: {msg}")
  },
  timeout(1000) => {
    output("Timed out")
  },
}
```
