# Userspace XNU harness

Apple ships a userspace test and fuzzing framework for the kernel in `xnu/tests/unit/`: the kernel is
built as a library (`RC_ProjectName=xnu_libraries XNU_LibAllFiles=1 XNU_LibFlavour=UNITTEST`), linked
into `libkernel.<config>.<machine>.dylib`, bootstrapped by `mocks/fake_kinit.c` through
`STARTUP_SUB_SYSCTL` with a real zone allocator, and driven by small test executables. The mocks
(~13.8k lines: pmap, vm_map, threads, IPC, ports, VFS, proc, hypervisor, KDP), a deterministic fibers
scheduler, `panic()` turned into a `longjmp`, and `T_FUZZ` entries that hand buffers to libFuzzer are
all in the public drop. What is not public is glue: darwintest, `embedded_device_map`, and the
`macosx.internal` SDK. This directory supplies that glue so the harness builds with Xcode alone.

## Status

**Working, and running Apple's own tests.** The kernel library links into a dylib, Apple's mocks link
against it, `fake_kinit()` runs the real kernel bootstrap in-process to `STARTUP_SUB_SYSCTL` (phase
16, the phase Apple's default plan targets), and `xnu/tests/unit/ipc/mach_port_construct.c` runs
**unmodified** and passes all 8 of its cases:

```
startup_phase=16, 8 test(s)
[1/8] reply_port_lifecycle
[2/8] ip_alloc_test
...
[7/8] mach_port_construct_stress_test
  log: Stress test completed: 286 total, 220 success, 66 failures
[8/8] mach_port_construct_port_types_with_options
PASS: 8 test(s)
```

Those tests exercise real Mach IPC: port construction across every port type, MPO flag combinations,
guard/immovable/filter semantics, and a 286-iteration stress loop. Two negative controls confirm the
results are not vacuous — a deliberately false assertion aborts with a `FAIL` line and a non-zero
exit, and `mach_port_destruct()` on a name that was never allocated is rejected by XNU's own IPC
validation rather than quietly succeeding.

Live in-process at that point: zalloc/kalloc on a real bootstrap, Mach IPC spaces, ports, rights and
vouchers, `vm_map` operations over mocked pmap and page layers, one fabricated proc/task, IOKit lock
statics, `panic()` as a `longjmp`, and the deterministic fibers scheduler.

Seven things were needed beyond Apple's own files, all in `attached.c`, `plan.c`, `build.sh` and
`kdk_zero_stubs.txt`:

1. **KDK-archive symbols.** `osfmk/conf/files.arm64` marks the pmap and low-level arm64 sources
   `optional nos_arm_asm`, and that option is off because the KDK archive normally supplies them, so
   they are compiled into neither the kernel nor the library. `attached.c` defines the 35 data
   symbols the library imports (classified data-vs-text with `nm -m` on `libT6020.os.DEVELOPMENT.a`,
   not guessed), an empty copyio recovery table, and traps for six pmap internals that only `pmap.c`
   defines. Linking the KDK's own `pmap.o` is impossible: its objects are the `arm64e.kernel`
   sub-architecture, which ld will not mix into an `arm64e` userspace link.
2. **A sysroot.** The recorded kernel flags carry none, so `-lc++` fails without `-isysroot`.
3. **Mocks the public drop cannot build.** `osfmk/arm64/hv/` is unpublished, so the three hypervisor
   mocks and `mock_mach_port.c` are skipped; `mock_pmap.c` sets `xprr_tpro_enabled`, a `struct pmap`
   field the public header lacks, so it is neutralized by a substitution that fails loudly if Apple
   changes the line.
4. **Low-address memory.** XNU packs vm_map, vm_map_entry and vm_object pointers into 31 bits with a
   6-bit shift from a 16 KiB base, and `mock_alloc.c` asserts packability, but `mock_mem.c` takes its
   pools from `calloc`, which macOS serves around 500 GB for large sizes. `harness_low_alloc` cuts
   them from one fixed reservation below the ~137 GB limit.
5. **`mmap` resolution.** That reservation must call libc's `mmap` through `dlsym(RTLD_NEXT)`: a
   plain call from inside the kernel dylib binds to XNU's own `mmap` syscall and crashes.
6. **Skipping `sop_page_pool_init`.** `osfmk/arm64/sop.c` registers it at
   `STARTUP(ZALLOC, STARTUP_RANK_LAST)`; it grabs pages with `vm_page_grab_options()` and asserts a
   non-zero physical address inside `[vm_first_phys, vm_last_phys)`, which a userspace process cannot
   satisfy. `plan.c` skips it through Apple's own `FAKE_KINIT_CUSTOMIZE_PLAN()` hook, so nothing in
   `xnu/tests/unit` is patched. Note the skip array must be `static`: the API stores the pointer and
   the plan runs after the hook returns, so the macro's compound literal would dangle.
7. **Zero stubs.** Seven KDK routines with no userspace meaning, listed in `kdk_zero_stubs.txt` and
   compiled into `zero_stubs.s` by `build.sh`. Every one is visible and auditable rather than hidden
   in a catch-all.

## Build

1. Build the kernel as a library. Apple's default product is an SPTM SoC (`j414c`, T6020); VMAPPLE
   compiles the non-SPTM pmap that Apple's `mock_pmap.c` does not target.

   ```fish
   env XNU_LIB_ALL_FILES=1 XNU_LIB_FLAVOUR=UNITTEST MACOS_VERSION=26.5 KERNEL_CONFIG=DEVELOPMENT ARCH_CONFIG=ARM64 MACHINE_CONFIG=T6020 ./build.sh --lib
   ```

2. Link the harness and run the smoke test (same `KERNEL_CONFIG`/`ARCH_CONFIG`/`MACHINE_CONFIG`):

   ```fish
   env KERNEL_CONFIG=DEVELOPMENT ARCH_CONFIG=ARM64 MACHINE_CONFIG=T6020 harness/build.sh
   build/harness/DEVELOPMENT_ARM64_T6020/sym/smoke
   ```

The library build is the slow step and only has to be repeated when instrumentation changes
(sanitizers and coverage go into xnu's `CFLAGS_EXTRA`). Harnesses are one compile and one link each.

## Layout

| File | Role | Apple equivalent |
|---|---|---|
| `build.sh` | prelink, kernel dylib, `func_unimpl.inc`, mocks dylib, side library, smoke | `xnu/tests/unit/Makefile` |
| `dt_stub.c` | darwintest replacement: the `dt_proxy_callbacks` on `raw_printf()`/`abort()` | `mocks/dt_proxy.c` + `libdarwintest.a` |
| `smoke.c` | prints the startup phase `fake_kinit()` reached, fails below `STARTUP_SUB_SYSCTL` | `example_test_osfmk.c` |
| `plan.c` | startup-plan customization via Apple's `FAKE_KINIT_CUSTOMIZE_PLAN()` hook | test-side hook |
| `attached.c` | KDK-archive data symbols, copyio table, pmap traps, low-address arena | `mocks/mock_attached.c` |
| `kdk_zero_stubs.txt` | the KDK routines stubbed to return 0; `build.sh` generates `zero_stubs.s` | — |
| `include/darwintest.h` | the darwintest macros Apple's tests use, so they compile unmodified | `libdarwintest.a` + its headers |
| `dt_runner.c` | `main()` that runs every registered `T_DECL`; aborts on the first failure | libdarwintest's runner |
| `include/dyld-interposing.h` | verbatim copy of the public dyld header (APSL 2.0), absent from the SDK | `$(SDKROOT)/usr/local/include/mach-o/dyld-interposing.h` |

Add a test by appending its path to `TESTS` in `build.sh`; each is one compile and one link against
the two dylibs, so the expensive library build is not repeated.

Next milestone: a fuzz target over the Mach port surface these tests already reach — a
`FuzzedDataProvider`-style loop over `mach_port_construct` / `mach_port_mod_refs` /
`mach_port_request_notification` / `mach_port_destruct` on `current_space()`, driven at first by a
plain `main(argc, argv)` reading a file, since there is no arm64e libFuzzer runtime on this host.

The mocks, `tools/quote_defines.py`, `tools/xnu_lib.unexport`, and `all-alias.exp` are used in place
from the xnu checkout and the library objdir, so they always match the source drop being built.

## What it does not do yet

- **Coverage-guided fuzzing.** Apple clang instruments (`-fsanitize=fuzzer-no-link`) but ships no
  libFuzzer runtime, and Homebrew LLVM's runtime has no arm64e slice. Options: build compiler-rt's
  `lib/fuzzer` for arm64e and wrap it in a dylib as Apple's Makefile does, or drive
  `-fsanitize-coverage=trace-pc-guard` from an external fuzzer. ASan/UBSan dylibs do ship with Xcode.
- **Buffer-backed `copyin`/`copyout`.** Apple's mocks are no-ops, so copyin-driven paths read
  uninitialized buffers. Replacing them is the single most enabling change for parser targets.
- **The rest of `xnu/tests/unit/`.** `include/darwintest.h` covers the macro surface
  `mach_port_construct.c` uses. Other tests will need more of it (`T_ASSERT_PANIC`, `T_MOCK_*`
  helpers, the fibers scheduler entry points); add them as they come up. EXPECT is currently as fatal
  as ASSERT, which can only turn a silent failure into a loud one.

## Targets, ranked by what Apple's harness already reaches

Easy: Mach port/right/notification state machine, vouchers (Apple's `ipc/` tests drive them in-process).
Easy-medium: sysctl, `vm_map` operation sequences. Medium: `OSUnserialize`, `ipc_kmsg` descriptor
parsing, MIG dispatch. Hard: VFS, Mach-O loader, kqueue, networking, DTrace DIF, kexts. Not reachable
through this library: CoreEntitlements, libDER, img4 (KDK binaries).
