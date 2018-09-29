import winim/inc/windef
import winim/inc/winuser
import ../types


proc SendInput(command: ptr INPUT): bool =
  SendInput(UINT(1), command, int32(sizeof(INPUT))) == 1


proc SendInput(command: MOUSEINPUT): bool =
  var input: INPUT
  input.`type` = INPUT_MOUSE
  input.union1.mi = command
  result = SendInput(addr(input))


proc handleMouseCommand*(command: MihoCommand): bool =
  result = true
  case command.kind:
    of mcMouseMove:
      var mi: MOUSEINPUT
      mi.dx = LONG(command.dx)
      mi.dy = LONG(command.dy)
      mi.mouseData = DWORD(0)
      mi.dwFlags = MOUSEEVENTF_MOVE
      mi.time = DWORD(0)
      mi.dwExtraInfo = ULONG_PTR(0)
      discard SendInput(mi)

    of mcMouseButton:
      var mi: MOUSEINPUT
      mi.dx = LONG(0)
      mi.dy = LONG(0)
      mi.mouseData = DWORD(0)
      mi.time = DWORD(0)
      mi.dwExtraInfo = ULONG_PTR(0)

      case command.button:
        of mmbLeft:
          mi.dwFlags =
            if command.isDown: MOUSEEVENTF_LEFTDOWN
            else: MOUSEEVENTF_LEFTUP
        of mmbRight:
          mi.dwFlags =
            if command.isDown: MOUSEEVENTF_RIGHTDOWN
            else: MOUSEEVENTF_RIGHTUP
        of mmbMiddle:
          mi.dwFlags =
            if command.isDown: MOUSEEVENTF_MIDDLEDOWN
            else: MOUSEEVENTF_MIDDLEUP
      
      discard SendInput(mi)

    else:
      result = false
