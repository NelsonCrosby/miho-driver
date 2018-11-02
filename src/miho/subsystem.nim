import nxcbor
export nxcbor.CborObject
export nxcbor.CborObjectKind


type
  Subsystem* = object of RootObj
    name*: string
  
  HandleResult* = tuple[status: int, response: CborObject]


proc newSubsystem*[T: Subsystem](name: string): ref T =
  new(result)
  result.name = name

method handleCommand*(
  subsystem: ref Subsystem,
  instruction: int,
  arguments: seq[CborObject]
): HandleResult {.base.} =
  result.status = -1
