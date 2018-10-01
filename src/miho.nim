import strutils
import asyncnet, asyncdispatch
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
      var msg: CborObject
      (complete, msg) = parser.parse()

      if complete:
        var items = msg.items
        let subsystem = items[0].value
        let command = items[1].value
        let arguments = items[2..items.high]

        let (respond, response) =
          miho.subsystems[subsystem].handleCommand(command, arguments)

        if respond:
          let res = response.encode()
          await client.send(res)


proc serve*(miho: MihoServer) {.async.} =
  miho.svrSock.listen()

  while true:
    let (address, client) = await miho.svrSock.acceptAddr()
    asyncCheck miho.handleClient(address, client)
