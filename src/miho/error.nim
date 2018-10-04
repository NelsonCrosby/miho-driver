import cbor


type
  MihoErrorCode* {.pure.} = enum
    message = -2,
    cborParse = -1,
    exception = 0,
    command,
    arguments,
    values,
  
  MihoError* = object of Exception
    code*: uint


proc newMihoError*(code: uint, msg: string): ref MihoError =
  result = newException(MihoError, msg)
  result.code = code


template checkCommand*(instruction: int) =
  raise newMihoError(
    uint(MihoErrorCode.command),
    "unknown command " & $instruction
  )


template checkLength*(args: seq[CborObject], expect: int) =
  if args.len != expect:
    raise newMihoError(
      uint(MihoErrorCode.arguments),
      "expected " & $expect & " arguments"
    )

template checkKind*(arg: CborObject, name: string, index: int, expect: CborObjectKind) =
  if arg.kind != expect:
    raise newMihoError(
      uint(MihoErrorCode.arguments),
      "expected argument " & name & " (" & $index & ") to be " & $expect
    )

template checkTrue*(name: string, index: int, message: string, cond: bool) =
  if not cond:
    raise newMihoError(
      uint(MihoErrorCode.values),
      "expected argument " & name & " (" & $index & ") to " & message
    )
