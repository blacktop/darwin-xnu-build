#!/usr/bin/env bash
#
# Build Apple's userspace XNU harness (xnu/tests/unit) with the public toolchain.
#
# This mirrors xnu/tests/unit/Makefile without its Apple-internal inputs (darwintest,
# embedded_device_map, the macosx.internal SDK):
#   1. prelink the xnu_libraries archive with Apple's unexport and alias lists
#   2. link it into an interposable libkernel.<config>.<machine>.dylib with the bootstrap attached
#   3. generate stubs for the imports the dylib still lacks (libsptm_xnu, TXM, Tightbeam)
#   4. build libmocks.dylib from xnu/tests/unit/mocks
#   5. build the darwintest replacement (dt_stub.c) and the smoke executable
#
# Prerequisite: a full library build, for example
#   env XNU_LIB_ALL_FILES=1 XNU_LIB_FLAVOUR=UNITTEST XNU_LIB_ARCH_STRING=arm64 MACOS_VERSION=26.5 \
#       KERNEL_CONFIG=DEVELOPMENT ARCH_CONFIG=ARM64 MACHINE_CONFIG=T6020 ./build.sh --lib
#
# Outputs land in build/harness[-<XNU_LIB_VARIANT>]/<CONFIG>_<ARCH>_<MACHINE>/{obj,sym}.
set -euo pipefail

WORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HARNESS_DIR="${WORK_DIR}/harness"
UNIT_DIR="${WORK_DIR}/xnu/tests/unit"
KERNEL_CONFIG="${KERNEL_CONFIG:-DEVELOPMENT}"
ARCH_CONFIG="${ARCH_CONFIG:-ARM64}"
MACHINE_CONFIG="${MACHINE_CONFIG:-T6020}"
# Extra flags for harness sources, e.g. the -D__BUILDING_WITH_*__ defines that match a
# sanitizer-instrumented library build.
HARNESS_CFLAGS="${HARNESS_CFLAGS:-}"
XNU_LIB_VARIANT="${XNU_LIB_VARIANT:-}"
GUIDED_FUZZING="${GUIDED_FUZZING:-0}"

if [[ "${XNU_LIB_VARIANT}" == *[![:alnum:]_.-]* ]]; then
    echo "[harness] error: XNU_LIB_VARIANT must contain only letters, digits, '.', '_' or '-'" >&2
    exit 1
fi
if [ "${GUIDED_FUZZING}" != 0 ] && [ "${GUIDED_FUZZING}" != 1 ]; then
    echo "[harness] error: GUIDED_FUZZING must be 0 or 1" >&2
    exit 1
fi

kernel_config_lc=$(echo "${KERNEL_CONFIG}" | tr '[:upper:]' '[:lower:]')
machine_config_lc=$(echo "${MACHINE_CONFIG}" | tr '[:upper:]' '[:lower:]')
variant_suffix="${XNU_LIB_VARIANT:+-${XNU_LIB_VARIANT}}"
OBJD="${WORK_DIR}/build/xnu-lib${variant_suffix}.obj/${KERNEL_CONFIG}_${ARCH_CONFIG}_${MACHINE_CONFIG}"
LIB_BASE="libkernel.${kernel_config_lc}.${machine_config_lc}"
XNU_LIB="${OBJD}/${LIB_BASE}.a"
OUT="${WORK_DIR}/build/harness${variant_suffix}/${KERNEL_CONFIG}_${ARCH_CONFIG}_${MACHINE_CONFIG}"
OBJ="${OUT}/obj"
SYM="${OUT}/sym"

info() {
    echo "[harness] $*"
}

die() {
    echo "[harness] error: $*" >&2
    exit 1
}

for required in "${XNU_LIB}" "${OBJD}/all-alias.exp" "${OBJD}/osfmk/${KERNEL_CONFIG}/.CFLAGS" \
    "${OBJD}/bsd/${KERNEL_CONFIG}/.CFLAGS" "${UNIT_DIR}/mocks/fake_kinit.c"; do
    [ -e "${required}" ] || die "missing ${required}; run the library build first (see the header of this script)"
done
if [ "$(ar -t "${XNU_LIB}" | grep -c '\.o$')" -lt 100 ]; then
    die "${XNU_LIB} holds almost no objects; rebuild with XNU_LIB_ALL_FILES=1 XNU_LIB_FLAVOUR=UNITTEST"
fi

CC=$(xcrun -find clang)
LD=$(xcrun -find ld)
LIBTOOL=$(xcrun -find libtool)
DYLD_INFO=$(xcrun -find dyld_info)
# The recorded kernel flags carry no sysroot (kernel code never links libSystem); the dylibs do.
SDKROOT=$(xcrun --show-sdk-path)
mkdir -p "${OBJ}/mocks" "${SYM}"

# Replay the compile flags xnu recorded for each component, exactly as Apple's Makefile does.
# tr strips the NUL bytes xnu leaves in the recorded .CFLAGS, which bash warns about otherwise.
eval "OSFMK_CFLAGS=( $(python3 "${UNIT_DIR}/tools/quote_defines.py" "${OBJD}/osfmk/${KERNEL_CONFIG}/.CFLAGS" | tr -d '\0') )"
eval "BSD_CFLAGS=( $(python3 "${UNIT_DIR}/tools/quote_defines.py" "${OBJD}/bsd/${KERNEL_CONFIG}/.CFLAGS" | tr -d '\0') )"
OSFMK_CFLAGS+=("-I${OBJD}/osfmk/${KERNEL_CONFIG}")
BSD_CFLAGS+=("-I${OBJD}/bsd/${KERNEL_CONFIG}")
COMMON_CFLAGS=("-isysroot" "${SDKROOT}" "-I${UNIT_DIR}" "-I${UNIT_DIR}/mocks" "-I${HARNESS_DIR}/include" "-I${OBJ}" -g
    -Wno-missing-prototypes -Wno-unused-parameter -Wno-missing-variable-declarations)
# shellcheck disable=SC2206 # HARNESS_CFLAGS is a user-supplied flag list; word splitting is intended
COMMON_CFLAGS+=(${HARNESS_CFLAGS})
if [ "${GUIDED_FUZZING}" = 1 ]; then
    case " ${OSFMK_CFLAGS[*]} " in *" -fsanitize=fuzzer-no-link "*) ;; *)
        die "GUIDED_FUZZING=1 needs an XNU library built with -fsanitize=fuzzer-no-link"
        ;;
    esac
    case " ${OSFMK_CFLAGS[*]} " in *" -fsanitize=address "*) ;; *)
        die "GUIDED_FUZZING=1 needs an XNU library built with -fsanitize=address"
        ;;
    esac
    COMMON_CFLAGS+=(-D__BUILDING_WITH_LIBFUZZER__=1 -D__BUILDING_WITH_SANCOV__=1
        -D__BUILDING_WITH_SANITIZER__=1 -D__BUILDING_WITH_ASAN__=1)
fi

# Adapt Apple's mocks to the public source drop:
#   - osfmk/arm64/hv/ (hypervisor) is not published, so the hv mocks and mock_mach_port.c, which
#     includes their headers, cannot compile. They only serve tests/unit/hypervisor.
#   - mock_pmap.c sets pmap->xprr_tpro_enabled, a struct pmap field the public headers lack.
# The substitution is verified so it can never turn into a silent no-op when Apple changes the line.
SKIPPED_MOCKS="osfmk/mock_hv.c osfmk/mock_hv_vm.c osfmk/mock_hv_vcpu.c osfmk/mock_mach_port.c"
mkdir -p "${OBJ}/mocks-src/osfmk"
sed 's/pmap->xprr_tpro_enabled = true;/(void)pmap; \/* xprr_tpro_enabled: not in the public struct pmap *\//' \
    "${UNIT_DIR}/mocks/osfmk/mock_pmap.c" >"${OBJ}/mocks-src/osfmk/mock_pmap.c"
if cmp -s "${UNIT_DIR}/mocks/osfmk/mock_pmap.c" "${OBJ}/mocks-src/osfmk/mock_pmap.c"; then
    die "mock_pmap.c substitution did not apply; Apple changed the pmap_set_tpro mock, update this script"
fi

# mock_mem.c's pools must live below XNU's pointer-packing limit; see harness_low_alloc in attached.c.
sed 's|pb->buffer = calloc(1, total_size);|{ extern void *harness_low_alloc(unsigned long); pb->buffer = harness_low_alloc(total_size); } /* harness: pointer-packing range */|' \
    "${UNIT_DIR}/mocks/mock_mem.c" >"${OBJ}/mocks-src/mock_mem.c"
if cmp -s "${UNIT_DIR}/mocks/mock_mem.c" "${OBJ}/mocks-src/mock_mem.c"; then
    die "mock_mem.c substitution did not apply; Apple changed the pool allocation, update this script"
fi

# Apple's unexport list plus the harness's own additions; see xnu_lib.unexport.extra for why.
cat "${UNIT_DIR}/tools/xnu_lib.unexport" "${HARNESS_DIR}/xnu_lib.unexport.extra" >"${OBJ}/xnu_lib.unexport"

info "prelinking ${LIB_BASE}.a"
"${CC}" "${OSFMK_CFLAGS[@]}" -c -x c /dev/null -o "${OBJ}/arch_def.o"
"${LD}" -r "${OBJ}/arch_def.o" -all_load "${XNU_LIB}" -o "${OBJ}/${LIB_BASE}.prelinked.a" \
    -unexported_symbols_list "${OBJ}/xnu_lib.unexport" -alias_list "${OBJD}/all-alias.exp"

FUZZ_LINK=()
SAN_ATTACHED=()
if [ "${GUIDED_FUZZING}" = 1 ]; then
    LIBFUZZER_RUNTIME="${LIBFUZZER_RUNTIME:-}"
    if [ -z "${LIBFUZZER_RUNTIME}" ]; then
        LIBFUZZER_RUNTIME=$(find /opt/homebrew/opt/llvm/lib/clang \
            -path '*/lib/darwin/libclang_rt.fuzzer_osx.a' -print -quit 2>/dev/null || true)
    fi
    [ -f "${LIBFUZZER_RUNTIME}" ] || die "GUIDED_FUZZING=1 needs an arm64 LIBFUZZER_RUNTIME"
    lipo -verify_arch arm64 "${LIBFUZZER_RUNTIME}" || die "${LIBFUZZER_RUNTIME} has no arm64 slice"

    # Keep libFuzzer in its own dylib so its libc calls cannot bind to XNU's same-named symbols.
    # Current inline-counter instrumentation also needs pcs_init, absent from Apple's export list.
    {
        cat "${UNIT_DIR}/tools/libfuzzer.export"
        echo ___sanitizer_cov_pcs_init
    } |
        sort -u >"${OBJ}/libfuzzer.export"
    "${LD}" -r "${OBJ}/arch_def.o" -all_load "${LIBFUZZER_RUNTIME}" \
        -exported_symbols_list "${OBJ}/libfuzzer.export" -o "${OBJ}/libfuzzer.prelinked.a"
    "${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -dynamiclib \
        "${OBJ}/libfuzzer.prelinked.a" -lc++ -install_name @rpath/libfuzzer.dylib \
        -o "${SYM}/libfuzzer.dylib" -U _LLVMFuzzerTestOneInput
    FUZZ_LINK=("${SYM}/libfuzzer.dylib")
    SAN_ATTACHED=("${UNIT_DIR}/mocks/san_attached.c")
fi

# KDK routines the bootstrap calls that have no meaning in a userspace process. Each is listed in
# kdk_zero_stubs.txt and becomes a function returning 0, instead of the trap that func_unimpl.inc
# would otherwise generate. Keeping the list in a file makes every one of them visible.
info "generating zero stubs from kdk_zero_stubs.txt"
{
    echo -e "// Generated by harness/build.sh from harness/kdk_zero_stubs.txt. Do not edit.\n\t.text\n\t.p2align 2"
    grep -v '^[[:space:]]*\(#.*\)\?$' "${HARNESS_DIR}/kdk_zero_stubs.txt" | sort -u |
        awk '{print "\t.globl _" $1 "\n_" $1 ":\n\tmov\tx0, #0\n\tret"}'
} >"${OBJ}/zero_stubs.s"

info "linking ${LIB_BASE}.dylib"
XNU_DYLIB="${SYM}/${LIB_BASE}.dylib"
"${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -dynamiclib "${FUZZ_LINK[@]}" \
    "${UNIT_DIR}/mocks/fake_kinit.c" "${UNIT_DIR}/mocks/fake_libsptm.c" "${UNIT_DIR}/mocks/mock_3rd_party.c" \
    "${OBJ}/mocks-src/mock_mem.c" "${UNIT_DIR}/mocks/mock_attached.c" "${HARNESS_DIR}/attached.c" \
    "${SAN_ATTACHED[@]}" "${OBJ}/zero_stubs.s" \
    -Wl,-all_load,"${OBJ}/${LIB_BASE}.prelinked.a" -lc++ -Wl,-undefined,dynamic_lookup -Wl,-interposable \
    -install_name "@rpath/${LIB_BASE}.dylib" -o "${XNU_DYLIB}"

# Whatever is still unresolved becomes a trap in mocks/osfmk/mock_unimpl.c, minus the zero stubs
# above, which are already defined in the dylib.
info "generating func_unimpl.inc from the dylib's unresolved imports"
"${DYLD_INFO}" -imports "${XNU_DYLIB}" >"${OBJ}/imports.txt"
if ! awk '$0 ~ /<flat-namespace>/ {flat++}
    $1 ~ /^_/ && $2 == "(from" && $3 == "<flat-namespace>)" {matched++}
    END {exit flat == 0 || flat != matched}' "${OBJ}/imports.txt"; then
    die "unrecognized dyld_info flat-namespace import record"
fi
{
    echo "// Generated from undefined imports of ${XNU_DYLIB}"
    awk '$1 ~ /^_/ && $2 == "(from" && $3 == "<flat-namespace>)" {print substr($1, 2)}' \
        "${OBJ}/imports.txt" |
        { grep -vxF -f "${HARNESS_DIR}/kdk_zero_stubs.txt" || true; } |
        sort -u |
        awk '{print "UNIMPLEMENTED(" $1 ")"}'
} >"${OBJ}/func_unimpl.inc"

info "compiling mocks"
mock_objects=()
while IFS= read -r src; do
    rel="${src#"${UNIT_DIR}/mocks/"}"
    case " ${SKIPPED_MOCKS} " in *" ${rel} "*) continue ;; esac
    src_dir_include=()
    if [ "${rel}" = "osfmk/mock_pmap.c" ]; then
        src="${OBJ}/mocks-src/osfmk/mock_pmap.c"
        src_dir_include=("-I${UNIT_DIR}/mocks/osfmk") # quoted includes resolve relative to the original file
    fi
    obj="${OBJ}/mocks/${rel%.c}.o"
    mkdir -p "$(dirname "${obj}")"
    case "${rel}" in
    osfmk/*) "${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" "${src_dir_include[@]}" -DUT_BUILDING_LIBMOCKS -c "${src}" -o "${obj}" ;;
    bsd/*) "${CC}" "${BSD_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -DUT_BUILDING_LIBMOCKS -c "${src}" -o "${obj}" ;;
    esac
    mock_objects+=("${obj}")
done < <(find "${UNIT_DIR}/mocks/osfmk" "${UNIT_DIR}/mocks/bsd" -name '*.c' | sort)
"${LIBTOOL}" -static "${mock_objects[@]}" -o "${OBJ}/libmocks.a"

info "linking libmocks.dylib"
MOCKS_DYLIB="${SYM}/libmocks.dylib"
"${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" "${XNU_DYLIB}" -Wl,-all_load,"${OBJ}/libmocks.a" -dynamiclib \
    -install_name @rpath/libmocks.dylib -o "${MOCKS_DYLIB}"
nm -gjU "${MOCKS_DYLIB}" | { grep '_MOCK_' || true; } >"${OBJ}/libmocks.mocksyms"

info "building the darwintest replacement and the startup-plan hook"
"${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -c "${HARNESS_DIR}/dt_stub.c" -o "${OBJ}/dt_stub.o"
# plan.c overrides fake_kinit.c's weak _fki_edit_plan_hook, so it must land in the executable.
"${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -c "${HARNESS_DIR}/plan.c" -o "${OBJ}/plan.o"
"${LIBTOOL}" -static "${OBJ}/dt_stub.o" "${OBJ}/plan.o" -o "${OBJ}/libside.a"

info "linking smoke"
"${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -DUT_MODULE=osfmk "${HARNESS_DIR}/smoke.c" \
    "${XNU_DYLIB}" "${MOCKS_DYLIB}" -Wl,-force_load,"${OBJ}/libside.a" -rpath @executable_path -o "${SYM}/smoke"
# Apple's own tests, compiled unmodified against harness/include/darwintest.h. Each is one compile
# plus one link against the two dylibs; the expensive library build is not repeated.
info "linking tests"
# Apple's tests, compiled unmodified. notification_policy.c and voucher_restrictions.c are
# deliberately absent: the first asserts behavior that only holds when CONFIG_ROSETTA is defined,
# the second reaches an SMR path that is unsupported in user mode. Both are explained under
# "Test status" in README.md; add them here to reproduce.
TESTS=(
    "${UNIT_DIR}/ipc/mach_port_construct.c"
    "${UNIT_DIR}/ipc/voucher_user_data.c"
    "${UNIT_DIR}/ipc/tss_policy.c"
    "${UNIT_DIR}/ipc/copyout_immovable_send_right.c"
    "${UNIT_DIR}/ipc/xpc_connection_port_pair.c"
)
for test_src in "${TESTS[@]}"; do
    test_name=$(basename "${test_src}" .c)
    "${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -DUT_MODULE=osfmk \
        "${test_src}" "${HARNESS_DIR}/dt_runner.c" \
        "${XNU_DYLIB}" "${MOCKS_DYLIB}" -Wl,-force_load,"${OBJ}/libside.a" \
        -rpath @executable_path -o "${SYM}/${test_name}"
    info "  ${SYM}/${test_name}"
done

# Fuzz targets keep the replay driver. Guided builds add a separate libFuzzer driver.
info "linking fuzz targets"
FUZZERS=(
    "${HARNESS_DIR}/fuzz_mach_port.c"
)
for fuzz_src in "${FUZZERS[@]}"; do
    fuzz_name=$(basename "${fuzz_src}" .c)
    "${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -DUT_MODULE=osfmk \
        "${fuzz_src}" "${HARNESS_DIR}/fuzz_runner.c" \
        "${XNU_DYLIB}" "${MOCKS_DYLIB}" -Wl,-force_load,"${OBJ}/libside.a" \
        -rpath @executable_path -o "${SYM}/${fuzz_name}"
    info "  ${SYM}/${fuzz_name}"
    if [ "${GUIDED_FUZZING}" = 1 ]; then
        "${CC}" "${OSFMK_CFLAGS[@]}" "${COMMON_CFLAGS[@]}" -DUT_MODULE=osfmk \
            "${fuzz_src}" "${HARNESS_DIR}/fuzz_libfuzzer_runner.c" \
            "${FUZZ_LINK[@]}" "${XNU_DYLIB}" "${MOCKS_DYLIB}" \
            -Wl,-force_load,"${OBJ}/libside.a" -rpath @executable_path \
            -o "${SYM}/${fuzz_name}_guided"
        info "  ${SYM}/${fuzz_name}_guided"
    fi
done

info "done: run ${SYM}/smoke"
