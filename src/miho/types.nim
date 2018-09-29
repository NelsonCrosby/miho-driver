type
  MihoMouseButton* = enum
    mmbLeft = 0, mmbRight = 1, mmbMiddle = 2

  MihoCommandKind* = enum
    mcEmpty,
    mcRetry,
    mcClose,
    mcMouseMove,
    mcMouseButton,

  MihoCommand* = object
    case kind*: MihoCommandKind
    of mcEmpty, mcRetry, mcClose:
      discard
    of mcMouseMove:
      dx*, dy*: int
    of mcMouseButton:
      button*: MihoMouseButton
      isDown*: bool
