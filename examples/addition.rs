fn add() :: Integer {
  let in :: Unknown = input("Number? ")
  let proved = !~Integer in
  // Proves input is integer, but panic mask with ~ tilde
  if (proved) {
    return in
  }
}

add()
