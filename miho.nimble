import strutils

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


proc buildType(apptype: string) =
  let cachedir = nimcacheDir().replace("_d", "shared_d")
  let header = cachedir & "/shared.h"
  let dllname = toDll "miho"
  exec "nim c --app:" & apptype & " --out:build/" & dllname & " --header src/mihopkg/shared.nim"
  rmFile "build/miho-driver.h"
  mvFile header, "build/miho-driver.h"

task buildShared, "build shared library":
  buildType "lib"
