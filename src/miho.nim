import asyncnet, asyncdispatch
import miho.types

when defined(windows):
  import miho/win/input


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object
    svrSock: AsyncSocket


proc getInt16(data: string, offset: int = 0): int16 =
  return (int16(data[offset]) shl 8) + (int16(data[offset + 1]))


proc parseCommand(data: string): tuple[command: MihoCommand, remaining: string] =
  template notEnoughData() =
    result.command.kind = mcEmpty
    result.remaining = data
    return

  if data.len == 0:
    notEnoughData
  
  var command: MihoCommand

  var pos = 0
  command.kind = MihoCommandKind(data[pos])
  pos += 1

  case command.kind:
    of mcEmpty:
      command.kind = mcRetry
    of mcRetry, mcClose:
      discard

    of mcMouseMove:
      if data.len < (pos + 4):
        notEnoughData
      command.dx = int(getInt16(data, pos))
      command.dy = int(getInt16(data, pos + 2))
      pos += 4

    of mcMouseButton:
      if data.len < (pos + 2):
        notEnoughData
      command.button = MihoMouseButton(data[pos])
      command.isDown = data[pos + 1] != char(0)
      pos += 2

  result.command = command
  result.remaining = data.substr(pos)


proc newMihoServer*(port: Port, address: string = ""): MihoServer =
  new(result)
  result.svrSock = newAsyncSocket(buffered = false)
  result.svrSock.setSockOpt(OptReuseAddr, true)
  result.svrSock.bindAddr(port, address)


proc handleCommand(miho: MihoServer; cmd: MihoCommand) =
  case cmd.kind:
    of mcEmpty, mcRetry:
      discard
    of mcClose:
      echo "closing"
    of mcMouseMove, mcMouseButton:
      discard handleMouseCommand(cmd)


proc handleClient(miho: MihoServer; address: string; client: AsyncSocket) {.async.} =
  var data = ""
  while true:
    let recv = await client.recv(1024)
    if recv.len == 0:
      client.close()
      echo "goodbye ", address
      return

    data &= recv
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
    echo "Hello ", address
    asyncCheck miho.handleClient(address, client)
