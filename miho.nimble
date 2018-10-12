# Package

version       = "0.0.0"
author        = "Nelson Crosby"
description   = "Desktop remote control"
license       = "BSD2"
srcDir        = "src"
binDir        = "build"
bin           = @[ "miho" ]

# Dependencies
requires "nim >= 0.18.0"

when defined(windows):
  requires "winim >= 2.5"


task test, "run tests":
  exec "nim c -r tests/testcbor"
