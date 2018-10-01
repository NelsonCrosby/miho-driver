import cbor
export cbor.CborObject
export cbor.CborObjectKind


type
  Subsystem* = object of RootObj
    name*: string
  
  HandleResult* = tuple[respond: bool, response: CborObject]


proc newSubsystem*[T: Subsystem](name: string): ref T =
  new(result)
  result.name = name

method handleCommand*(
  subsystem: ref Subsystem,
  instruction: int,
  arguments: seq[CborObject]
): HandleResult {.base.} =
  result.respond = false
