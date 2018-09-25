import asyncnet, asyncdispatch


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object
    svrSock: AsyncSocket

  MihoCommandKind = enum
    mcEmpty,
    mcClose,
    mcMoveMouse,
    mcClickMouse,

  MihoMouseButton = enum
    mmbLeft = 0, mmbRight = 1

  MihoCommand* = object
    case kind: MihoCommandKind
    of mcEmpty, mcClose:
      discard
    of mcMoveMouse:
      dx, dy: int
    of mcClickMouse:
      button: MihoMouseButton


proc parseCommand(data: string): tuple[command: MihoCommand, remaining: string] =
  discard


proc newMihoServer*(port: Port, address: string = ""): MihoServer =
  new(result)
  result.svrSock = newAsyncSocket()
  result.svrSock.setSockOpt(OptReuseAddr, true)
  result.svrSock.bindAddr(port, address)


proc handleCommand(miho: MihoServer; cmd: MihoCommand) =
  case cmd.kind:
    of mcEmpty:
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
