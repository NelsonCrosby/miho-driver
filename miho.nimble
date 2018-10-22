import strutils

# Package

version       = "0.0.0"
author        = "Nelson Crosby"
description   = "Desktop remote control"
license       = "BSD2"
srcDir        = "src"
binDir        = "build"

# Dependencies
requires "nim >= 0.18.0"

when defined(windows):
  requires "winim >= 2.5"


task buildCli, "build command-line driver":
  let cachedir = nimcacheDir().replace("_d", "miho_d")
  let exename = toExe "miho"
  mkDir "build"
  exec "nim c --app:" & apptype & " --out:build/" & exename & " src/bin/miho.nim"

proc buildType(apptype: string) =
  let cachedir = nimcacheDir().replace("_d", "shared_d")
  let header = cachedir & "/shared.h"
  let dllname = toDll "miho"
  mkDir "build"
  exec "nim c --app:" & apptype & " --out:build/" & dllname & " --header src/miho/shared.nim"
  rmFile "build/miho-driver.h"
  mvFile header, "build/miho-driver.h"

task buildShared, "build shared library":
  buildType "lib"
