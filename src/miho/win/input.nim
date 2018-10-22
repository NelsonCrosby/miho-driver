import winim/inc/windef
import winim/inc/winuser


type
  MouseButton* = enum
    mmbLeft, mmbRight, mmbMiddle


proc SendInput(command: ptr INPUT): bool =
  SendInput(UINT(1), command, int32(sizeof(INPUT))) == 1


proc SendInput(command: MOUSEINPUT): bool =
  var input: INPUT
  input.`type` = INPUT_MOUSE
  input.union1.mi = command
  result = SendInput(addr(input))


proc mouseMove*(dx: int, dy: int) =
  var mi: MOUSEINPUT
  mi.dx = LONG(dx)
  mi.dy = LONG(dy)
  mi.mouseData = DWORD(0)
  mi.dwFlags = MOUSEEVENTF_MOVE
  mi.time = DWORD(0)
  mi.dwExtraInfo = ULONG_PTR(0)
  discard SendInput(mi)

proc mouseButton*(btn: MouseButton, isDown: bool) =
  var mi: MOUSEINPUT
  mi.dx = LONG(0)
  mi.dy = LONG(0)
  mi.mouseData = DWORD(0)
  mi.time = DWORD(0)
  mi.dwExtraInfo = ULONG_PTR(0)

  mi.dwFlags =
    case btn:
      of mmbLeft:
        if isDown: MOUSEEVENTF_LEFTDOWN
        else: MOUSEEVENTF_LEFTUP
      of mmbRight:
        if isDown: MOUSEEVENTF_RIGHTDOWN
        else: MOUSEEVENTF_RIGHTUP
      of mmbMiddle:
        if isDown: MOUSEEVENTF_MIDDLEDOWN
        else: MOUSEEVENTF_MIDDLEUP
  
  discard SendInput(mi)
