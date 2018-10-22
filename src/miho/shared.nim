## Shared library support
## ======================
##
## This module implements C bindings the Miho driver. It is expected
## that this file will be compiled with ``--app:lib``. The resulting
## shared library should be reasonably easily usable from another
## languages' FFI.

import asyncdispatch

import server


type
  FFIMihoServer {.exportc: "miho_server_t".} = object
    miho: MihoServer
    serveFut: Future[void]


proc miho_new*(port: cint): ptr FFIMihoServer {.exportc, dynlib.} =
  result = create(FFIMihoServer)
  result.miho = newMihoServer(Port(port))
  result.serveFut = nil

proc miho_free*(fms: ptr FFIMihoServer) {.exportc, dynlib.} =
  dealloc(fms)

proc miho_start*(fms: ptr FFIMihoServer) {.exportc, dynlib.} =
  if fms.serveFut.isNil:
    fms.serveFut = fms.miho.serve()

proc miho_wait*(fms: ptr FFIMihoServer) {.exportc, dynlib.} =
  miho_start(fms)
  waitFor fms.serveFut
  fms.serveFut = nil

proc miho_stop*(fms: ptr FFIMihoServer) {.exportc, dynlib.} =
  fms.miho.stop()
