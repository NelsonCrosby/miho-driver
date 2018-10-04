type
  MihoErrorCode* {.pure.} = enum
    messageType = -2,
    cborParse = -1,
    exception = 0,
  
  MihoError* = object of Exception
    code: uint


proc newMihoError*(code: uint, msg: string): ref MihoError =
  result = newException(MihoError, msg)
  result.code = code
