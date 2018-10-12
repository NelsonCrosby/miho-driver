import strutils
import parseopt
import asyncdispatch
import asyncfile
import mihopkg.server
import mihopkg.cbor


type
  RpcCommand = enum
    rpcStart = 1,
    rpcStop = 2,
  
  RpcEvent = enum
    rpcStarted = 1,
    rpcStopped = 2,
    rpcConnect = 3,
    rpcDisconnect = 4,


proc write(file: AsyncFile, obj: CborObject) {.async.} =
  await file.write(obj.encode())

proc hostRpc(): Future[int] {.async.} =
  var
    stdin = newAsyncFile(AsyncFD(0))
    stdout = newAsyncFile(AsyncFD(1))
    server: MihoServer = nil
    serverLoop: Future[void] = nil

  result = QuitSuccess

  var parser = newCborParser()

  while true:
    let recv = await stdin.read(1024)
    if recv.len == 0:
      return

    parser.add(recv)
    var (complete, msg) = parser.parse()
    if not complete:
      stderr.writeLine("incomplete cbor object")
      return QuitFailure

    assert msg.kind == cboArray
    assert msg.items.len > 0
    assert msg.items[0].kind == cboInteger

    let cmd = msg.items[0].value
    case cmd:
      of int(rpcStart):
        var port = MihoDefaultPort
        if msg.items.len > 1:
          assert msg.items[1].kind == cboInteger
          port = Port(msg.items[1].value)

          server = newMihoServer(port)
          
          server.onConnect proc(address: string) {.async.} =
            await stdout.write(CborObject(kind: cboArray, items: @[
              CborObject(kind: cboInteger, value: int(rpcConnect)),
              CborObject(kind: cboString, isText: true, data: address)
            ]))
          server.onDisconnect proc(address: string) {.async.} =
            await stdout.write(CborObject(kind: cboArray, items: @[
              CborObject(kind: cboInteger, value: int(rpcDisconnect)),
              CborObject(kind: cboString, isText: true, data: address)
            ]))

          serverLoop = server.serve()

          await stdout.write(CborObject(kind: cboArray, items: @[
            CborObject(kind: cboInteger, value: int(rpcStarted))
          ]))

      of int(rpcStop):
        server.stop()
        await serverLoop
        await stdout.write(CborObject(kind: cboArray, items: @[
          CborObject(kind: cboInteger, value: int(rpcStopped))
        ]))

      else:
        stderr.writeLine("invalid command ", cmd)
        return QuitFailure


proc help() =
  echo """
usage: miho_driver [options]

options:
  --port, -p:     Set port to use
  --rpc:          Start RPC on stdio (for interfaces)
"""
  quit(QuitSuccess)


proc badOption(key, val: string) =
  echo "unknown option ", key
  quit(QuitFailure)


var port = MihoDefaultPort

var opt = initOptParser()
for kind, key, val in opt.getopt():
  case kind:
    of cmdArgument:
      discard

    of cmdLongOption:
      case key:
        of "help": help()
        of "port": port = Port(val.parseInt())
        of "rpc": quit(waitFor hostRpc())
        else: badOption(key, val)

    of cmdShortOption:
      case key:
        of "h": help()
        of "p": port = Port(val.parseInt())
        else: badOption(key, val)

    of cmdEnd:
      break

var svr = newMihoServer(port)
asyncCheck svr.serve()
runForever()
