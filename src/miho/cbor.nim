import lists


type
  CborItemKind* = enum
    cbPositive = 0,
    cbNegative = 1,
    cbByteString = 2,
    cbTextString = 3,
    cbArray = 4,
    cbMap = 5,
    cbTag = 6,
    cbSimple = 7,
    # Starting here are specified with cbSimple in protocol,
    # but the data part is parsed and understood.
    cbFloatHalf,
    cbFloatSingle,
    cbFloatDouble,
    cbBreak,
    # This is a signal that there was not enough data to complete the comand.
    cbIncomplete,

  CborItem* = object
    case kind*: CborItemKind:
    of cbPositive, cbNegative:
      value*: uint64
    of cbByteString, cbTextString:
      data*: string
    of cbArray, cbMap:
      bounded*: bool
      nItems*: uint64
    of cbTag:
      tag*: uint64
    of cbSimple:
      info*: uint8
    of cbFloatHalf, cbFloatSingle:
      valueFloat*: float32
    of cbFloatDouble:
      valueDouble*: float64
    of cbBreak, cbIncomplete:
      discard

  CborObjectKind* = enum
    cboInteger,
    cboString,
    cboArray,
    cboTable,
    cboFloat,
    cboBoolean,
    cboNull,
    cboUndefined,

  CborFloatKind* {.pure.} = enum half, single, double

  CborObject* = object
    case kind*: CborObjectKind:
    of cboInteger: value*: int
    of cboString:
      isText*: bool
      data*: string
    of cboArray: items*: seq[CborObject]
    of cboTable: entries*: seq[tuple[key, value: CborObject]]
    of cboFloat:
      kindFloat*: CborFloatKind
      valueFloat*: float
    of cboBoolean: enabled*: bool
    of cboNull, cboUndefined: discard

  CborParser* = ref object
    data: string
    offset: int
    stack: DoublyLinkedList[CborParserState]

  CborParserStateItemKind = enum skNew, skArray, skMap
  CborParserState = object
    item: CborItem
    index: int
    case kind: CborParserStateItemKind:
    of skNew:
      discard
    of skArray:
      items: seq[CborObject]
    of skMap:
      keyStored: bool
      entries: seq[tuple[key, value: CborObject]]


proc decodeItemInfo(data: string, offset: int):
      tuple[cborType, info: byte; infoValue: uint64; pos: int] =
  var pos = offset

  template ensureEnoughData(need: int) =
    if data.len < (pos + need):
      result.pos = -1
      return

  ensureEnoughData(1)

  let leading = byte(data[pos])
  pos += 1

  result.cborType = leading shr 5
  result.info = leading and 31
  result.infoValue = 0

  if result.info < 24:
    result.infoValue = result.info
  elif result.info == 24:
    ensureEnoughData(1)
    result.infoValue = uint64(data[pos])
    pos += 1
  elif result.info == 25:
    ensureEnoughData(2)
    result.infoValue = (
      (uint64(data[pos]) shr 8) +
      (uint64(data[pos + 1]))
    )
    pos += 2
  elif result.info == 26:
    ensureEnoughData(4)
    result.infoValue = (
      (uint64(data[pos]) shr 8) +
      (uint64(data[pos + 1]) shr 8) +
      (uint64(data[pos + 2]) shr 8) +
      (uint64(data[pos + 3]))
    )
    pos += 4
  elif result.info == 27:
    ensureEnoughData(8)
    result.infoValue = (
      (uint64(data[pos]) shr 8) +
      (uint64(data[pos + 1]) shr 8) +
      (uint64(data[pos + 2]) shr 8) +
      (uint64(data[pos + 3]) shr 8) +
      (uint64(data[pos + 4]) shr 8) +
      (uint64(data[pos + 5]) shr 8) +
      (uint64(data[pos + 6]) shr 8) +
      (uint64(data[pos + 7]))
    )
    pos += 8

  result.pos = pos

proc nextCborItem*(data: string, offset: int = 0):
      tuple[item: CborItem, position: int] =
  var (cborType, info, infoValue, pos) = decodeItemInfo(data, offset)

  template ensureEnoughData(need: int) =
    if pos < 0 or data.len < (pos + need):
      result.item.kind = cbIncomplete
      result.position = offset
      return

  ensureEnoughData(0)   # pos < 0 if decodeItemInfo couldn't complete

  if cborType < 7:
    let kind = CborItemKind(cborType)
    result.item.kind = kind

    case kind:
      of cbPositive, cbNegative:
        result.item.value = infoValue
      of cbByteString, cbTextString:
        let length = int(infoValue)
        ensureEnoughData(length)
        result.item.data = data.substr(pos, pos + length)
        pos += length
      of cbArray, cbMap:
        if info == 31:
          result.item.bounded = false
          result.item.nItems = 0
        else:
          result.item.bounded = true
          result.item.nItems = infoValue
      of cbTag:
        result.item.tag = infoValue
      else:
        discard

  else:
    # Other items (e.g. simple values, floats, ...)
    if info <= 24:
      result.item.kind = cbSimple
      result.item.info = uint8(infoValue)
    elif info == 25:
      assert false, "TODO: half prec. float"
    elif info == 26:
      result.item.kind = cbFloatSingle
      result.item.valueFloat = cast[float32](uint32(infoValue))
    elif info == 27:
      result.item.kind = cbFloatDouble
      result.item.valueDouble = cast[float64](infoValue)
    elif info == 31:
      result.item.kind = cbBreak
    else:
      assert false, "unassigned value of 7:" & $info

  result.position = pos

proc encode*(item: CborItem): string =
  var info = -1
  var infoValue: uint64
  var extra = ""

  case item.kind:
    of cbPositive, cbNegative:
      infoValue = item.value
    of cbByteString, cbTextString:
      infoValue = uint64(item.data.len)
      extra = item.data
    of cbArray, cbMap:
      if item.bounded:
        infoValue = item.nItems
      else:
        info = 31
    of cbTag:
      infoValue = item.tag
    of cbSimple:
      infoValue = item.info
    of cbFloatHalf:
      assert false, "TODO: Half prec. float"
    of cbFloatSingle:
      infoValue = uint64(cast[uint32](item.valueFloat))
    of cbFloatDouble:
      infoValue = cast[uint64](item.valueDouble)
    of cbBreak:
      info = 31
    of cbIncomplete:
      return

  if info < 0:
    if infoValue < 24:
      info = int(infoValue)
    elif infoValue <= uint8.high:
      info = 24
    elif infoValue <= uint16.high:
      info = 25
    elif infoValue <= uint32.high:
      info = 26
    elif infoValue <= 0xFFFF_FFFF_FFFF_FFFF'u64:
      info = 27

  let kind =
    if item.kind < cbSimple:
      int(item.kind)
    else: 7
  let leading = uint8((kind shl 5) + info)

  var value = ""
  if info == 24:
    value = $char(infoValue and 0xFF)
  elif info == 25:
    value = (
      char((infoValue shr 8) and 0xFF) &
      char(infoValue and 0xFF)
    )
  elif info == 26:
    value = (
      char((infoValue shr 24) and 0xFF) &
      char((infoValue shr 16) and 0xFF) &
      char((infoValue shr 8) and 0xFF) &
      char(infoValue and 0xFF)
    )
  elif info == 26:
    value = (
      char((infoValue shr 56) and 0xFF) &
      char((infoValue shr 48) and 0xFF) &
      char((infoValue shr 40) and 0xFF) &
      char((infoValue shr 32) and 0xFF) &
      char((infoValue shr 24) and 0xFF) &
      char((infoValue shr 16) and 0xFF) &
      char((infoValue shr 8) and 0xFF) &
      char(infoValue and 0xFF)
    )

  return char(leading) & value & extra


proc pop[T](list: var DoublyLinkedList[T]): T =
  let node = list.tail
  list.tail = node.prev
  if list.head == node:
    list.head = nil
  node.prev = nil
  result = node.value


proc newCborParser*(): CborParser =
  new(result)
  result.data = ""
  result.offset = 0
  result.stack = initDoublyLinkedList[CborParserState]()

proc add*(parser: CborParser, data: string, offset: int = 0, length: int = -1) =
  if length < 0:
    parser.data &= data.substr(offset)
  else:
    parser.data &= data.substr(offset, offset + length)


proc emptyState(item: CborItem): CborParserState =
  result.item = item
  result.index = 0
  result.kind = skNew

proc saveState(parser: CborParser, item: CborItem,
               index: int, items: seq[CborObject]) =
  var state: CborParserState
  state.item = item
  state.index = index
  state.kind = skArray
  state.items = items

  parser.stack.append(state)

proc saveState(parser: CborParser, item: CborItem,
               index: int, keySet: bool,
               entries: seq[tuple[key, value: CborObject]]) =
  var state: CborParserState
  state.item = item
  state.index = index
  state.kind = skMap
  state.entries = entries

  parser.stack.append(state)

proc parseItem(parser: CborParser, state: CborParserState):
      tuple[complete: bool, obj: CborObject]

proc parse(parser: CborParser, forceNew: bool): tuple[complete: bool, obj: CborObject] =
  if forceNew:
    var item: CborItem
    (item, parser.offset) = nextCborItem(parser.data, parser.offset)
    parser.stack.append(emptyState(item))

  while not parser.stack.tail.isNil:
    var state = parser.stack.pop()
    result = parser.parseItem(state)
    if not result.complete:
      return

  parser.data = parser.data.substr(parser.offset)
  parser.offset = 0

proc parse*(parser: CborParser): tuple[complete: bool, obj: CborObject] =
  parser.parse(parser.stack.tail.isNil)

proc parseItem(parser: CborParser, state: CborParserState):
      tuple[complete: bool, obj: CborObject] =
  let item = state.item
  result.complete = true

  case item.kind:
    of cbIncomplete:
      result.complete = false
      return

    of cbPositive:
      result.obj.kind = cboInteger
      result.obj.value = int(item.value)
    of cbNegative:
      result.obj.kind = cboInteger
      result.obj.value = -1 - int(item.value)

    of cbByteString, cbTextString:
      result.obj.kind = cboString
      result.obj.data = item.data
      result.obj.isText = item.kind == cbTextString

    of cbArray:
      if not item.bounded:
        assert false, "TODO: Can't yet parse unbounded arrays"

      result.obj.kind = cboArray

      var index: int
      var items: seq[CborObject]
      if state.kind == skNew:
        index = 0
        items.newSeq(item.nItems)
      else:
        index = state.index
        items = state.items

      for i in index..items.high:
        let (complete, obj) = parser.parse(true)
        if not complete:
          parser.saveState(item, i, items)
          result.complete = false
          return
        items[i] = obj

      result.obj.items = items

    of cbMap:
      if not item.bounded:
        assert false, "TODO: Can't yet parse unbounded maps"

      result.obj.kind = cboTable

      var index: int
      var entries: seq[tuple[key, value: CborObject]]
      var keyStored: bool
      if state.kind == skNew:
        index = 0
        entries.newSeq(state.item.nItems)
        keyStored = false
      else:
        index = state.index
        entries = state.entries
        keyStored = state.keyStored

      for i in index..entries.high:
        var complete: bool

        if not keyStored:
          (complete, entries[i].key) = parser.parse(true)
          if not complete:
            parser.saveState(item, i, false, entries)
            result.complete = false
            return

        (complete, entries[i].value) = parser.parse(true)
        if not complete:
          parser.saveState(item, i, true, entries)
          result.complete = false
          return

      result.obj.entries = entries

    of cbTag:
      assert false, "TODO: Don't understand tags yet :("

    of cbSimple:
      case item.info:
        of 20:
          result.obj.kind = cboBoolean
          result.obj.enabled = false
        of 21:
          result.obj.kind = cboBoolean
          result.obj.enabled = true
        of 22:
          result.obj.kind = cboNull
        of 23:
          result.obj.kind = cboUndefined
        else:
          assert false, "Don't understand simple value " & $item.info

    of cbBreak:
      assert false, "Unexpected break :("

    of cbFloatHalf, cbFloatSingle:
      result.obj.kind = cboFloat
      result.obj.valueFloat = item.valueFloat
      result.obj.kindFloat =
        if item.kind == cbFloatHalf: CborFloatKind.half
        else: CborFloatKind.single

    of cbFloatDouble:
      result.obj.kind = cboFloat
      result.obj.valueFloat = item.valueDouble
      result.obj.kindFloat = CborFloatKind.double


proc item*(obj: CborObject): CborItem =
  case obj.kind:
    of cboInteger:
      if obj.value < 0:
        result.kind = cbNegative
        result.value = uint64(-obj.value) - 1
      else:
        result.kind = cbPositive
        result.value = uint64(obj.value)

    of cboString:
      result.kind =
        if obj.isText: cbTextString
        else: cbByteString
      result.data = obj.data

    of cboArray:
      result.kind = cbArray
      result.bounded = true
      result.nItems = uint64(obj.items.len)

    of cboTable:
      result.kind = cbMap
      result.bounded = true
      result.nItems = uint64(obj.entries.len)

    of cboFloat:
      result.kind =
        case obj.kindFloat:
          of half: cbFloatHalf
          of single: cbFloatSingle
          of double: cbFloatDouble
      result.valueDouble = float64(obj.valueFloat)

    of cboBoolean:
      result.kind = cbSimple
      result.info =
        if obj.enabled: 21
        else: 20

    of cboNull:
      result.kind = cbSimple
      result.info = 22
    of cboUndefined:
      result.kind = cbSimple
      result.info = 23

proc encode*(obj: CborObject): string =
  result = obj.item.encode()
  case obj.kind:
    of cboArray:
      for ob in obj.items:
        result &= ob.item.encode()
    of cboTable:
      for i, pair in obj.entries:
        result &= pair.key.item.encode()
        result &= pair.value.item.encode()
    else:
      discard
