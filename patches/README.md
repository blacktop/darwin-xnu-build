# Patch Catalog

This repository already ships a number of pre-made patches that soften the rough edges of Apples open-source XNU drops. The sets fall into three groups: version-specific bundles, cross-version compatibility layers, and single-purpose hotfixes. Below is a quick reference so you can decide which pieces to reuse when advancing to newer macOS releases.

## macOS 26.0 Toolkit (`patches/26.0/`)

### Minimal Required Pieces
- `00_setup_coreentitlements.sh`: copies KDK CoreEntitlements V2 headers, creates the missing `Kernel.h`, and stubs `os/firehose_buffer_private.h`.
- `add_iboot_header.patch`: provides the stubbed `EXTERNAL_HEADERS/iBoot/boot_args_abi.h`.
- `link_kdk.patch`: **CRITICAL** – force-loads the KDK archive (`libVMAPPLE.os.RELEASE.a`) so the kernel links all private ARM64 monitor/MMU routines.
- `remove_applefeatures_include.patch`: drops the unavailable `<AppleFeatures.h>` include from `vm_resident.c`.
- `remove_tightbeam.patch`: clears `config/libTightbeam.exports` so the linker stops looking for private `_tb_*` entry points.

### Optional Quality-of-Life Patches
- `disable_bti_vmapple.patch`: undefines `BTI_ENFORCED` just for VMAPPLE so BTI checks don't trip when booting the kernel inside Virtualization.framework guests.

> Historical note: `disable_dtrace_vm_apple.patch` was used on older drops to sidestep assembler issues in SDT macros. The macOS 26.0 toolchain now builds without it, so the patch has been retired.

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
