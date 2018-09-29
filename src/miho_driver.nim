import asyncdispatch
import miho

var svr = newMihoServer(Port(1234))
asyncCheck svr.serve()
runForever()
