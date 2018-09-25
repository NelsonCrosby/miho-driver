import asyncdispatch
import asyncnet


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object

  MihoCommandKind = enum
    mcMoveMouse,
    mcClickMouse,

  MihoMouseButton = enum
    mmbLeft = 0, mmbRight = 1

  MihoCommand* = object
    case kind: MihoCommandKind
    of mcMoveMouse:
      dx, dy: int
    of mcClickMouse:
      button: MihoMouseButton


proc newMihoServer*(): MihoServer =
  new(result)


proc handleCommand*(miho: MihoServer; cmd: MihoCommand) =
  case cmd.kind:
    of mcMoveMouse:
      echo "mouse moved"
    of mcClickMouse:
      echo "mouse clicked"


proc serve*(miho: MihoServer): Future[void] {.async.} =
  discard
