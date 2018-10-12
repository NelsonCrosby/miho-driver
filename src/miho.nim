import strutils
import parseopt
import asyncdispatch
import mihopkg.server


proc help() =
  echo """
usage: miho_driver [options]

options:
  --port, -p:     Set port to use
"""
  quit(0)


proc badOption(key, val: string) =
  echo "unknown option ", key
  quit(1)


var port = Port(6446)

var opt = initOptParser()
for kind, key, val in opt.getopt():
  case kind:
    of cmdArgument:
      discard

    of cmdLongOption:
      case key:
        of "help": help()
        of "port": port = Port(val.parseInt())
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
