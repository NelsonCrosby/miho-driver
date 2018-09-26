import asyncnet, asyncdispatch


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object
    svrSock: AsyncSocket

  MihoMouseButton = enum
    mmbLeft = 0, mmbRight = 1

  MihoCommandKind = enum
    mcEmpty,
    mcRetry,
    mcClose,
    mcMoveMouse,
    mcClickMouse,

  MihoCommand* = object
    case kind: MihoCommandKind
    of mcEmpty, mcRetry, mcClose:
      discard
    of mcMoveMouse:
      dx, dy: int
    of mcClickMouse:
      button: MihoMouseButton


proc parseCommand(data: string): tuple[command: MihoCommand, remaining: string] =
  result.remaining = data
  if data.len == 0:
    result.command.kind = mcEmpty
    return

  var pos = 0
  result.command.kind = MihoCommandKind(data[pos])
  pos += 1

  case result.command.kind:
    of mcRetry, mcClose:
      return
    of mcEmpty:
      result.command.kind = mcRetry
      return
    of mcMoveMouse:
      if data.len < (pos + 2):
        result.command.kind = mcEmpty
        return
      result.command.dx = int(data[pos])
      result.command.dy = int(data[pos + 1])
      pos += 2
    of mcClickMouse:
      if data.len < (pos + 1):
        result.command.kind = mcEmpty
        return
      result.command.button = MihoMouseButton(data[pos])
      pos += 1

  result.remaining = data.substr(pos)


proc newMihoServer*(port: Port, address: string = ""): MihoServer =
  new(result)
  result.svrSock = newAsyncSocket()
  result.svrSock.setSockOpt(OptReuseAddr, true)
  result.svrSock.bindAddr(port, address)


proc handleCommand(miho: MihoServer; cmd: MihoCommand) =
  case cmd.kind:
    of mcEmpty, mcRetry:
      discard
    of mcClose:
      echo "closing"
    of mcMoveMouse:
      echo "mouse moved"
    of mcClickMouse:
      echo "mouse clicked"


proc handleClient(miho: MihoServer; address: string; client: AsyncSocket) {.async.} =
  var data = ""
  while true:
    data &= await client.recv(1024)
    while data.len > 0:
      var command: MihoCommand
      (command, data) = parseCommand(data)
      miho.handleCommand(command)

      if command.kind == mcEmpty:
        break       # This is a special internal kind that signals
                    # there's not enough data to parse the command
      elif command.kind == mcClose:
        client.close()
        return


proc serve*(miho: MihoServer) {.async.} =
  miho.svrSock.listen()

  while true:
    let (address, client) = await miho.svrSock.acceptAddr()
    asyncCheck miho.handleClient(address, client)
