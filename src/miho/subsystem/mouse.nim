import ../error
import ../subsystem

when defined(windows):
  import ../win/input
else:
  import ../stub/input


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
  result.status = -1

  case MouseCommandKind(instruction):
    of mcMove:
      arguments.checkLength(2)
      arguments[0].checkKind("dx", 0, cboInteger)
      arguments[1].checkKind("dy", 1, cboInteger)

      let dx = arguments[0].value
      let dy = arguments[1].value
      mouseMove(dx, dy)

    of mcButton:
      arguments.checkLength(2)
      arguments[0].checkKind("button", 0, cboInteger)
      arguments[1].checkKind("down", 1, cboBoolean)

      let btn = arguments[0].value
      let isDown = arguments[1].enabled

      checkTrue("button", 0, "be a valid button",
        btn == int(mmbLeft) or
        btn == int(mmbRight) or
        btn == int(mmbMiddle))

      mouseButton(MouseButton(btn), isDown)

    else:
      checkCommand(instruction)
