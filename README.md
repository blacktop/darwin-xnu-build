# darwin-xnu-build

[![XNU CodeQL](https://github.com/blacktop/darwin-xnu-build/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/blacktop/darwin-xnu-build/actions/workflows/c-cpp.yml)

> This repository contains the xnu src as well as scripts to build it as well as generate a kernel collection and a CodeQL database.

---

## Dependencies

- [homebrew](https://brew.sh)
  - [jq](https://stedolan.github.io/jq/)
  - [gum](https://github.com/charmbracelet/gum)
  - [xcodes](https://github.com/RobotsAndPencils/xcodes)
  - [ipsw](https://github.com/blacktop/ipsw)
- XCode
- python3
- [codeql CLI](https://codeql.github.com/docs/codeql-cli/)

## Supported OS Versions

- macOS 13.0
- macOS 13.1
- macOS 13.2

## Why? 🤔

I'm hoping to patch and build the xnu source in interesting ways to aid in research and development of macOS/iOS security research tools as well as generate [CodeQL](https://securitylab.github.com/tools/codeql) databases for the community to use.

## Getting Started

Clone the repo

```bash
git clone https://github.com/blacktop/darwin-xnu-build.git
cd darwin-xnu-build
```

```bash
❯ ./build.sh --help

Usage: build.sh [-h] [--clean] [--kc]

This script builds the macOS XNU kernel

Where:
    -h|--help       show this help text
    -c|--clean      cleans build artifacts and cloned repos
    -k|--kc         create kernel collection (via kmutil create)
```    

Build the kernel and kernel Collection

```bash
KERNEL_CONFIG=RELEASE ARCH_CONFIG=ARM64 MACHINE_CONFIG=T6000 ./build.sh --kc
```

> **Note**  
> Supported `KERNEL_CONFIG` include: `RELEASE` and `DEVELOPMENT`  
> Supported `MACHINE_CONFIG` include: `T8101`, `T8103`, `T6000` and `VMAPPLE`  

```bash
<SNIP>
 ⇒ 📦 Building kernel collection for 'kernel.release.t6000'
   • Decompressing KernelManagement kernelcache
Merged LINKEDIT:
  weak bindings size:          0KB
  exports info size:           0KB
  bindings size:               0KB
  lazy bindings size:          0KB
  function starts size:       41KB
  data in code size:           0KB
  symbol table size:        3702KB (85348 exports, 87979 imports)
  symbol string pool size:  6465KB
LINKEDITS optimized from 30MB to 10MB
time to layout cache: 0ms
time to copy cached dylibs into buffer: 1ms
time to adjust segments for new split locations: 2ms
time to bind all images: 8ms
time to optimize Objective-C: 0ms
time to do stub elimination: 0ms
time to optimize LINKEDITs: 2ms
time to compute slide info: 1ms
time to compute UUID and codesign cache file: 1ms
  🎉 XNU Build Done!
```  
```bash
❯ ipsw macho info build/oss-xnu.kc | head
Magic         = 64-bit MachO
Type          = FILESET
CPU           = AARCH64, ARM64e 
Commands      = 241 (Size: 17160)
Flags         = None
000: LC_UUID                     67DF7148-8EEC-B1A6-5F51-7502DADF2264
001: LC_BUILD_VERSION            Platform: unknown, SDK: 0.0
002: LC_UNIXTHREAD               Threads: 1, ARM64 EntryPoint: 0xfffffe0007ad1488
003: LC_DYLD_CHAINED_FIXUPS      offset=0x003690000  size=0x444
004: LC_SEGMENT_64 sz=0x00008000 off=0x00000000-0x00008000 addr=0xfffffe0007004000-0xfffffe000700c000 r--/r--   __TEXT  
<SNIP>
```

Clean rebuild the kernel and kernel collection

```bash
KERNEL_CONFIG=RELEASE ARCH_CONFIG=ARM64 MACHINE_CONFIG=T6000 ./build.sh --clean --kc
```

Generate a CodeQL database

```bash
./codeql.sh
```
```bash
<SNIP>
[2023-03-03 22:33:20] [build-stdout]   🎉 XNU Build Done!
Finalizing database at darwin-xnu-build/xnu-codeql.
Running TRAP import for CodeQL database at darwin-xnu-build/xnu-codeql...
TRAP import complete (1m46s).
Successfully created database at darwin-xnu-build/xnu-codeql.
[info] Deleting log files...
[info] Zipping the CodeQL database...
  🎉 CodeQL Database Create Done!
```

Zips up the CodeQL database

```bash
❯ ll xnu-codeql.zip 
-rw-r--r--@ 1 blacktop  staff   219M Mar  3 22:35 xnu-codeql.zip
```

## TODO

- [x] Auto build xnu with Github Actions
- [x] Auto generate CodeQL database with Github Actions

## NOTES

To see kernel logs

```bash
log show --debug --last boot --predicate 'process == "kernel"'
```

## Credit

- <https://github.com/pwn0rz/xnu-build>
- <https://kernelshaman.blogspot.com/2021/02/building-xnu-for-macos-112-intel-apple.html>
