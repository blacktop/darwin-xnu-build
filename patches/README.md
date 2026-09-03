# Patch Catalog

This repository already ships a number of pre-made patches that soften the rough edges of Apples open-source XNU drops. The sets fall into three groups: version-specific bundles, cross-version compatibility layers, and single-purpose hotfixes. Below is a quick reference so you can decide which pieces to reuse when advancing to newer macOS releases.

## macOS 26.0 Toolkit (`patches/26.0/`)

### Minimal Required Pieces
- `00_setup_coreentitlements.sh`: copies the CoreEntitlements V2 headers from the KDK selected by `build.sh` and creates the missing `Kernel.h`. It also deletes the obsolete `os/firehose_buffer_private.h` stub that earlier revisions generated (see the firehose notes below).
- `add_iboot_header.patch`: provides the stubbed `EXTERNAL_HEADERS/iBoot/boot_args_abi.h`.
- `link_kdk.patch`: **CRITICAL** – force-loads the KDK archive (`libVMAPPLE.os.RELEASE.a`) so the kernel links all private ARM64 monitor/MMU routines.
- `remove_applefeatures_include.patch`: drops the unavailable `<AppleFeatures.h>` include from `vm_resident.c`.
- `dsymutil_no_process_substitution.patch`: runs `dsymutil` directly instead of through a bash process substitution, which failed with `EPERM` on `/dev/fd`.
- `simplify_san_lipo.patch`: replaces the `lipo -detailed_info | awk` pipeline in the sanitizer symbolset rule with a plain `lipo -create`, avoiding `_SC_ARG_MAX` failures.

### Optional Quality-of-Life Patches
- `disable_bti_vmapple.patch`: undefines `BTI_ENFORCED` just for VMAPPLE so BTI checks don't trip when booting the kernel inside Virtualization.framework guests.

> Historical note: `disable_dtrace_vm_apple.patch` was used on older drops to sidestep assembler issues in SDT macros. The macOS 26.0 toolchain now builds without it, so the patch has been retired. `remove_tightbeam.patch`, `iokit.patch` and `skywalk.patch` were likewise dropped from the 26.0 toolkit in `2b72aa9` once the 26.x sources stopped needing them.

## Sonoma / Sequoia Backports (`patches/15.0/` and `patches/14.4/`)
- `entitlements.patch`: routes legacy `CodeSignature/Entitlements.h` uses to the CoreEntitlements header and injects the missing web browser entitlement constants.
- `iokit.patch`: exports `IORPCMessageFromMach()` to remove reliance on private SDK glue in both DriverKit and the kernel IOKit user server.
- `skywalk.patch` (and the Ventura/Sonoma variants at the repo root): reintroduces the Field Packet Descriptor helpers (`kern_packet_set_fpd_*`) and the backing metadata fields so older networking clients keep compiling.
- `link_kdk.patch` & `link_kdk146.patch`: force-load the SoC-specific static archive from an installed KDK (`lib<MACHINE_CONFIG>.os.<KERNEL_CONFIG>.a`) for targets such as `VMAPPLE`; this is how we previously pulled in Apples binary-only monitor/pmap routines.
- `remove_tightbeam.patch`: empties the Tightbeam export list, ensuring the linker never looks for private `_tb_*` entry points.
- `restore_files.sh`: helper script that checks out deleted headers (TrustCache, DriverKit RPC) from known-good Apple release tags.

Both the 14.4 and 15.0 directories contain their own copies of the entitlements, IORPC, skywalk, and TrustCache headers so that older build targets can cherry-pick the right diff without dragging 26-specific assumptions along.

## Cross-Version Hotfixes (`patches/*.patch`)
- `machine_routines.patch`: adjusts `nonspeculative_timebase()` to read `CNTVCT` via the generic `S3_4_c15_c10_6` alias, which the public LLVM accepts.
- `kas_info.patch`: increases `KAS_INFO_MAX_SELECTOR` and tolerates the "special segment" selectors that Apples tooling expects, which keeps DTrace and CoreSymbolication happy.
- `iobuffermemd_monterey.patch`: fixes a false-success return in `IOBufferMemoryDescriptor::initWithPhysicalMask()`.
- `skywalk_ventura.patch` / `skywalk_sonoma.patch` / `syntax_checker_*`: mirror the networking helpers plus loosen LLDBs Python syntax checks so style scripts stop failing in OSS builds.
- `TrustCache.patch`: vendors the public TrustCache headers and inline helpers after Apple stopped shipping them.

## 26.0 Migration Notes

The macOS 26.0 (Tahoe beta) port required porting several critical patches from the 15.0 toolkit:

1. **KDK Linking**: The `link_kdk.patch` is essential - without it, the build fails with ~100+ undefined symbols including `_arm64_thread_exception_return`, `_mmu_kvtop`, `_rorgn_*`, and DART helpers. The KDK archive contains Apple's binary-only implementations of these low-level platform functions.

2. **Stub Conflicts**: A new `remove_ml_stubs_conflicts.patch` was needed because the original `ml_stubs.c` duplicated symbols now provided by the KDK. The patch keeps only stubs for functions truly missing from the KDK (like user JOP key helpers).

3. **Tightbeam**: VMAPPLE doesn't use Tightbeam, so the export list must be emptied to prevent linker errors.

4. **Skywalk & IOKit**: Both patches from 15.0 apply cleanly to 26.0 sources.

Having this catalog lets us spot when a new breakage matches an old fix. For future macOS releases, the immediate candidates to check are the KDK force-load rule, the Tightbeam export wipe, the skywalk FPD helpers, and any new stub conflicts with KDK archives.

## 26.5 Update Notes

The macOS 26.5 source drop (`xnu-12377.121.6`) keeps using the 26.0 fallback toolkit. Two older workarounds are now present upstream: `vm_resident.c` no longer includes `<AppleFeatures.h>`, and `MakeInc.def` already force-loads the matching KDK archive for non-`NONE` machine configs. The fallback patch loop will therefore skip those obsolete diffs while still applying the pieces that remain relevant.

## Silent patch skips and the firehose header (26.x)

Two build-time problems kept 26.x VMAPPLE kernels from booting even though `build.sh` reported success:

- `disable_bti_vmapple.patch` and `simplify_san_lipo.patch` shipped with a bare `@@` hunk header. `git apply --check` rejects that with "patch with only garbage", and the patch loop in `build.sh` discarded stderr and moved on, so neither patch was ever applied. Both hunks now carry real line counts, and the loop prints a warning for every patch it skips. A hunk header must look like `@@ -<start>,<count> +<start>,<count> @@`; `git apply` tolerates a stale start line (it searches for the context) but not a malformed header.
- `00_setup_coreentitlements.sh` used to generate a hand-written `os/firehose_buffer_private.h` that defined `FIREHOSE_BUFFER_KERNEL_CHUNK_COUNT` as `1`, the default chunk count as `64` and the default I/O page count as `0`. libdispatch defines the chunk count as the boot-arg driven variable `__firehose_buffer_kernel_chunk_count` (default 16) with 8 I/O pages, and `libkern/os/log.c` copies the boot log chunk into slot `CHUNK_COUNT - 1`, so the stub made xnu write the boot chunk over the firehose buffer header while `libfirehose_kernel.a` expected it in the last slot. The stub only existed because the `INCFLAGS_SDK` rewrite in `build.sh` no longer matched Apple's `MakeInc.def` (26.0 and 26.1 dropped the `kernel` path component, 26.2 and later replaced the tab with spaces), so the real header that libdispatch installs into `fakeroot/usr/local/include/kernel` was never on the include path. The rewrite now matches every known form, the stub is gone, and the script deletes any copy left behind by older runs.

`build.sh` now also passes `KDKROOT` to the patch scripts explicitly. Before, the variable was never exported, so the setup script always fell through to its glob fallback and copied the CoreEntitlements headers from whichever `KDK_26.*` sorted last on disk rather than the KDK being built against.

## What the KDK archive supplies (26.x arm64)

Apple's `makedefs/MakeInc.def` force-loads `$(KDKROOT)/System/Library/KernelSupport/lib<MACHINE_CONFIG>.os.<KERNEL_CONFIG>.a` for every machine config other than `NONE`, and `osfmk/conf/files.arm64` marks the matching sources `optional nos_arm_pmap` / `optional nos_arm_asm`, so they are never compiled from the open-source drop. In a `DEVELOPMENT_ARM64_VMAPPLE` kernel built from xnu-12377.121.6 against KDK 26.5 (25F71), these objects come from `libVMAPPLE.os.DEVELOPMENT.a` and are Apple's binaries, not code you can patch or instrument from source:

`start.o locore.o cswitch.o pcb.o pinst.o caches_asm.o gxf_exceptions.o machine_routines_asm.o machine_routines_apple.o machine_routines_nos.o pmap.o pmap_cs.o pmap_data.o pmap_iommu.o pmap_misc.o pmap_ppl_interface.o amcc_rorgn_common.o amcc_rorgn_pv_ctrr.o amcc_rorgn_ppl.o amcc_rorgn_ppl_amcc.o sart.o uat.o nvmeppl.o t8020dart.o t6000dart.o t8110dart.o sptm_stubs.o IOUnifiedAddressTranslator.cpo`

That is the whole pmap and PPL layer, the boot entry and exception vectors, context switching, cache maintenance, the AMCC/CTRR read-only region code, the DART/SART/UAT IOMMU drivers and the NVMe PPL. The KDK also supplies `libTightbeam.a` (force-loaded from `usr/local/lib/kernel/platform`). Anything else in the kernel is compiled from the published sources. Check what a given build linked with `cat build/xnu.obj/<CONFIG>/kernel.<variant>.<machine>.link/*.linkarchives` and `ar -t` on the archive. The idea of recording this map came from jonhermansen/darnix (`nix/default.nix`), which needs the list to build arm64 kernels without the KDK force-load.
