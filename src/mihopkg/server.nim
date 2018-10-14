import strutils
import asyncnet, asyncdispatch
import error
import cbor
import subsystem
import subsystem/mouse


const MihoDefaultPort* = Port(6446)


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object
    svrSock: AsyncSocket
    subsystems: seq[ref Subsystem]
    stopping: bool
    events: MihoServerEvents

  MihoClientEvent* = proc(address: string): Future[void]
  MihoServerEvents = object
    connect: MihoClientEvent
    disconnect: MihoClientEvent

  MihoCoreSubsystem = object of Subsystem
    svr: MihoServer


method handleCommand*(
  core: ref MihoCoreSubsystem,
  instruction: int,
  arguments: seq[CborObject]
): HandleResult =
  result.status = 0
  result.response.kind = cboArray
  result.response.items.newSeq(core.svr.subsystems.len - 1)
  for i in 1..core.svr.subsystems.high:
    var item: CborObject
    item.kind = cboString
    item.isText = true
    item.data = core.svr.subsystems[i].name
    result.response.items[i - 1] = item


proc onConnect*(miho: MihoServer, callback: MihoClientEvent) =
  miho.events.connect = callback

proc onDisconnect*(miho: MihoServer, callback: MihoClientEvent) =
  miho.events.disconnect = callback

proc newMihoServer*(port: Port, address: string = ""): MihoServer =
  new(result)
  result.stopping = false

  result.svrSock = newAsyncSocket(buffered = false)
  result.svrSock.setSockOpt(OptReuseAddr, true)
  result.svrSock.bindAddr(port, address)

  var coreSubsystem = newSubsystem[MihoCoreSubsystem](":")
  coreSubsystem.svr = result
  result.subsystems = @[
    coreSubsystem,
    newMouseSubsystem(),
  ]

  result.onConnect proc(address: string) {.async.} =
    echo "hello ", address
  result.onDisconnect proc(address: string) {.async.} =
    echo "goodbye ", address


proc createError(code: int, message: string): CborObject =
  result.kind = cboArray
  result.items.newSeq(2)
  result.items[0].kind = cboInteger
  result.items[0].value = code
  result.items[1].kind = cboString
  result.items[1].isText = true
  result.items[1].data = message


proc handleClient(miho: MihoServer; address: string; client: AsyncSocket) {.async.} =
  await miho.events.connect(address)
  var parser = newCborParser()

  while true:
    if miho.stopping:
      await miho.events.disconnect(address)
      client.close()
      return

    let recv = await client.recv(1024)
    if recv.len == 0:
      await miho.events.disconnect(address)
      client.close()
      return

    parser.add(recv)
    var complete = true
    while complete:
      var
        msg: CborObject
        error: bool = false
      try:
        (complete, msg) = parser.parse()
      except CborParseError:
        error = true
        msg = createError(int(MihoErrorCode.cborParse), getCurrentExceptionMsg())
      except Exception:
        error = true
        msg = createError(int(MihoErrorCode.cborParse), "internal error")
        when not defined(release):
          let exc = getCurrentException()
          stderr.writeLine("Traceback (most recent call last; client ", address, ")")
          stderr.write(exc.getStackTrace())
          stderr.writeLine(exc.name, ": ", exc.msg)

      if error:
        await client.send(msg.encode())
        await miho.events.disconnect(address)
        client.close()
        return

      if complete:
        var errorMsg = ""
        if msg.kind != cboArray:
          error = true
          errorMsg = "message must be of kind array"
        elif msg.items.len < 2:
          error = true
          errorMsg = "message must be at least 2 items long"
        elif msg.items[0].kind != cboInteger:
          error = true
          errorMsg = "message subsystem (0) must be of kind integer"
        elif msg.items[1].kind != cboInteger:
          error = true
          errorMsg = "message command (1) must be of kind integer"

        if error:
          msg = createError(int(MihoErrorCode.message), errorMsg)
          await client.send(msg.encode())
          await miho.events.disconnect(address)
          client.close()
          return

        var items = msg.items
        let subsystem = items[0].value
        let command = items[1].value
        let arguments = items[2..items.high]

        var
          status: int
          respond = true
          response: CborObject
        try:
          (status, response) =
            miho.subsystems[subsystem].handleCommand(command, arguments)
          respond = status >= 0
        except MihoError:
          error = true
          status = -1
          let exc = (ref MihoError)(getCurrentException())
          response = createError(int(exc.code), exc.msg)
        except Exception:
          error = true
          status = -1
          let exc = getCurrentException()
          response = createError(int(MihoErrorCode.exception), "internal error")
          when not defined(release):
            stderr.writeLine("Traceback (most recent call last; client ", address, ")")
            stderr.write(exc.getStackTrace())
            stderr.writeLine(exc.name, ": ", exc.msg)

        if respond:
          var message: CborObject
          message.kind = cboArray
          message.items.newSeq(3)
          message.items[0].kind = cboInteger
          message.items[0].value = subsystem
          message.items[1].kind = cboInteger
          message.items[1].value = status
          message.items[2] = response

          let msg = message.encode()
          await client.send(msg)
          if error:
            await miho.events.disconnect(address)
            client.close()
            return


proc remove[T](a: var seq[T], value: T): int {.discardable.} =
  result = -1
  for idx, it in a.pairs:
    if it == value:
      result = idx
      break

  if result >= 0:
    a.delete(result)


proc serve*(miho: MihoServer) {.async.} =
  miho.svrSock.listen()
  var clients: seq[Future[void]] = @[]

  while not miho.stopping:
    let (address, client) = await miho.svrSock.acceptAddr()
    let cfut = miho.handleClient(address, client)
    clients.add(cfut)
    cfut.addCallback proc (f: Future[void]) =
      echo "removing ", clients.remove(f)
  
  for cfut in clients:
    await cfut


proc stop*(miho: MihoServer) =
  miho.stopping = true
