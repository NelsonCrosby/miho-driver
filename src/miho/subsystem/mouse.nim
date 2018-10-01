import ../subsystem

when defined(windows):
  import ../win/input


type
  MouseSubsystem* = object of Subsystem

  MouseCommandKind = enum
    mcMove = 3,
    mcButton = 4,


proc newMouseSubsystem*(): ref Subsystem =
  newSubsystem[MouseSubsystem](":mouse")

method handleCommand*(
  subsystem: ref Subsystem,
  instruction: int,
  arguments: seq[CborObject]
): HandleResult =
  result.respond = false

  case MouseCommandKind(instruction):
    of mcMove:
      assert arguments[0].kind == cboInteger
      assert arguments[1].kind == cboInteger
      let dx = arguments[0].value
      let dy = arguments[1].value
      mouseMove(dx, dy)

    of mcButton:
      assert arguments[0].kind == cboInteger
      assert arguments[1].kind == cboBoolean
      let btn = arguments[0].value
      let isDown = arguments[1].enabled
      mouseButton(MouseButton(btn), isDown)
