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
  result.status = 0
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
  result.items.newSeq(2)
  result.items[0].kind = cboInteger
  result.items[0].value = code
  result.items[1].kind = cboString
  result.items[1].isText = true
  result.items[1].data = message


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
        client.close()
        echo "goodbye ", address, " (error in parsing cbor)"
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
          client.close()
          echo "goodbye ", address, " (invalid message)"
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
            client.close()
            echo "goodbye ", address,
              " (error in processing command ", subsystem, ":", command, ")"
            return


proc serve*(miho: MihoServer) {.async.} =
  miho.svrSock.listen()

  while true:
    let (address, client) = await miho.svrSock.acceptAddr()
    asyncCheck miho.handleClient(address, client)
