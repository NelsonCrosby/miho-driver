import strutils
import asyncnet, asyncdispatch
import miho.error
import miho.cbor
import miho.subsystem
import miho.subsystem.mouse


type
  MihoServer* = ref MihoServerObj
  MihoServerObj = object
    svrSock: AsyncSocket
    subsystems: seq[ref Subsystem]
  
  MihoCoreSubsystem = object of Subsystem
    svr: MihoServer


method handleCommand*(
  core: ref MihoCoreSubsystem,
  instruction: int,
  arguments: seq[CborObject]
): HandleResult =
  result.respond = true
  result.response.kind = cboArray
  result.response.items.newSeq(core.svr.subsystems.len - 1)
  for i in 1..core.svr.subsystems.high:
    var item: CborObject
    item.kind = cboString
    item.isText = true
    item.data = core.svr.subsystems[i].name
    result.response.items[i - 1] = item


proc newMihoServer*(port: Port, address: string = ""): MihoServer =
  new(result)

  result.svrSock = newAsyncSocket(buffered = false)
  result.svrSock.setSockOpt(OptReuseAddr, true)
  result.svrSock.bindAddr(port, address)

  var coreSubsystem = newSubsystem[MihoCoreSubsystem](":")
  coreSubsystem.svr = result
  result.subsystems = @[
    coreSubsystem,
    newMouseSubsystem(),
  ]


proc createError(code: int, message: string): CborObject =
  result.kind = cboArray
  result.items.newSeq(4)
  result.items[0].kind = cboInteger
  result.items[0].value = 0
  result.items[1].kind = cboInteger
  result.items[1].value = -1
  result.items[2].kind = cboInteger
  result.items[2].value = code
  result.items[3].kind = cboString
  result.items[3].isText = true
  result.items[3].data = message


proc handleClient(miho: MihoServer; address: string; client: AsyncSocket) {.async.} =
  echo "hello ", address
  var parser = newCborParser()

  while true:
    let recv = await client.recv(1024)
    if recv.len == 0:
      client.close()
      echo "goodbye ", address
      return

    parser.add(recv)
    var complete = true
    while complete:
      var
        msg: CborObject
        error: bool = false
      try:
        (complete, msg) = parser.parse()
      except AssertionError:
        error = true
        msg = createError(int(MihoErrorCode.cborParse), getCurrentExceptionMsg())
        when not defined(release):
          let exc = getCurrentException()
          stderr.writeLine("Traceback (most recent call last; client ", address, ")")
          stderr.write(exc.getStackTrace())
          stderr.writeLine(exc.name, ": ", exc.msg)
      except Exception:
        error = true
        msg = createError(int(MihoErrorCode.cborParse), getCurrentExceptionMsg())

      if error:
        await client.send(msg.encode())
        client.close()
        echo "goodbye ", address, " (error in parsing cbor)"
        return

      if complete:
        if msg.kind != cboArray:
          msg = createError(int(MihoErrorCode.messageType), "message must be of kind array")
          await client.send(msg.encode())
          client.close()
          echo "goodbye ", address, " (invalid message type)"
          return

        var items = msg.items
        let subsystem = items[0].value
        let command = items[1].value
        let arguments = items[2..items.high]

        var
          respond: bool
          response: CborObject
        try:
          (respond, response) =
            miho.subsystems[subsystem].handleCommand(command, arguments)
        except Exception:
          error = true
          respond = true
          let exc = getCurrentException()
          response = createError(int(MihoErrorCode.exception), exc.msg)
          when not defined(release):
            stderr.writeLine("Traceback (most recent call last; client ", address, ")")
            stderr.write(exc.getStackTrace())
            stderr.writeLine(exc.name, ": ", exc.msg)

        if respond:
          let res = response.encode()
          await client.send(res)
          if error:
            client.close()
            echo "goodbye ", address,
              " (error in processing command ", subsystem, ":", command, ")"
            return


proc serve*(miho: MihoServer) {.async.} =
  miho.svrSock.listen()

  while true:
    let (address, client) = await miho.svrSock.acceptAddr()
    asyncCheck miho.handleClient(address, client)
