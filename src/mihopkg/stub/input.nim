
type
  MouseButton* = enum
    mmbLeft, mmbRight, mmbMiddle


proc mouseMove*(dx: int, dy: int) =
  echo "input.mouseMove(", dx, ", ", dy, ")"

proc mouseButton*(btn: MouseButton, isDown: bool) =
  echo "input.mouseButton(", btn, ", ", isDown, ")"
