#!/usr/bin/env bash

# CREDIT: https://github.com/pwn0rz/xnu-build

set -o errexit
set -o nounset
set -o pipefail
if [[ "${TRACE-0}" == "1" ]]; then
    set -o xtrace
fi

# Help
if [[ "${1-}" =~ ^-*h(elp)?$ ]]; then
    echo 'Usage: build.sh [-h] [--clean]

This script builds the macOS XNU kernel

Where:
    -h       show this help text
    --kc     Create Kext Collection (via kmutil create)
    --clean  cleans build and fakeroot directories
'
    exit
fi

# Colors
export ESC_SEQ="\x1b["
export COL_RESET=$ESC_SEQ"39;49;00m"
export COL_RED=$ESC_SEQ"31;01m"
export COL_GREEN=$ESC_SEQ"32;01m"
export COL_YELLOW=$ESC_SEQ"33;01m"
export COL_BLUE=$ESC_SEQ"34;01m"
export COL_MAGENTA=$ESC_SEQ"35;01m"
export COL_CYAN=$ESC_SEQ"36;01m"

function running() {
    echo -e "$COL_MAGENTA ⇒ $COL_RESET"$1
}

function info() {
    echo -e "$COL_BLUE[info]$COL_RESET" $1
}

function error() {
    echo -e "$COL_RED[error] $COL_RESET"$1
}

: ${KERNEL_CONFIG:=RELEASE}
: ${ARCH_CONFIG:=ARM64}
: ${MACHINE_CONFIG:=VMAPPLE}
: ${JSONDB:=0}

WORK_DIR="$PWD"
BUILD_DIR=${WORK_DIR}/build
FAKEROOT_DIR=${WORK_DIR}/fakeroot
DSTROOT=${FAKEROOT_DIR}

KERNEL_FRAMEWORK_ROOT='/System/Library/Frameworks/Kernel.framework/Versions/A'

: ${RELEASE_URL:='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-132/release.json'}
: ${KDKROOT:='/Library/Developer/KDKs/KDK_13.2_22D49.kdk'}


install_deps() {
    if [ ! -x "$(command -v jq)" ] || [ ! -x "$(command -v gum)" ] || [ ! -x "$(command -v xcodes)" ]; then
        running "Installing dependencies"
        if [ ! -x "$(command -v brew)" ]; then
            error "Please install homebrew - https://brew.sh (or install 'jq' and 'gum' manually)"
        fi
        brew install jq gum xcodes
    fi
    if [ ! -d "/Applications/Xcode.app" ] || [ ! -d "/Applications/Xcode-beta.app" ]; then
        running "Installing XCode"
        gum style --border normal --margin "1" --padding "1 2" --border-foreground 212 "Choose $(gum style --foreground 212 'XCode') to install:"
        XCODE_VERSION=$(gum choose "13.4.1" "14.0.1" "14.1" "14.2" "14.3-beta")
        curl -o /tmp/Xcode_${XCODE_VERSION}.xip "https://storage.googleapis.com/xcodes-cache/Xcode_${XCODE_VERSION}.xip"
        xcodes install ${XCODE_VERSION} --experimental-unxip --path /tmp/Xcode_${XCODE_VERSION}.xip
        xcodebuild -downloadAllPlatforms
        xcodebuild -runFirstLaunch
    fi
}

choose_xnu() {
    gum style --border normal --margin "1" --padding "1 2" --border-foreground 212 "Choose $(gum style --foreground 212 'xnu') version to install:"
    XNU_VERSION=$(gum choose "macOS 13.0" "macOS 13.1" "macOS 13.2")
    case ${XNU_VERSION} in
    'macOS 13.0')
        RELEASE_URL='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-130/release.json'
        KDK_NAME='Kernel Debug Kit 13.0 build 22A380'
        KDKROOT='/Library/Developer/KDKs/KDK_13.0_22A380.kdk'
        ;;
    'macOS 13.1')
        RELEASE_URL='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-131/release.json'
        KDK_NAME='Kernel Debug Kit 13.1 build 22C65'
        KDKROOT='/Library/Developer/KDKs/KDK_13.1_22C65.kdk'
        ;;
    'macOS 13.2')
        RELEASE_URL='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-132/release.json'
        KDK_NAME='Kernel Debug Kit 13.2 build 22D49'
        KDKROOT='/Library/Developer/KDKs/KDK_13.2_22D49.kdk'
        ;;
    *)
        error "Invalid xnu version"
        exit 1
        ;;
    esac
    if [ ! -d "$KDKROOT" ]; then
        KDK_URL=$(curl "https://raw.githubusercontent.com/dortania/KdkSupportPkg/gh-pages/manifest.json" | jq -r '.[] | select(.name == "$KDK_NAME") | .url')
        running "Downloading $KDK_NAME to /tmp"
        (cd /tmp && curl -O "$KDK_URL")
        info "Please install KDK manually and retry the script after installation"
    fi
}

venv() {
    if [ ! -d "${WORK_DIR}/venv" ]; then
        running "Creating virtual environment"
        python3 -m venv ${WORK_DIR}/venv
    fi
    info "Activating virtual environment"
    source ${WORK_DIR}/venv/bin/activate
}

get_xnu() {
    if [ ! -d "${WORK_DIR}/xnu" ]; then
        running "⬇️ Cloning xnu"
        XNU_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="xnu") | .tag')
        git clone --branch ${XNU_VERSION} https://github.com/apple-oss-distributions/xnu.git ${WORK_DIR}/xnu
    fi
}

patches() {
    running "Patching xnu files"
    # xnu headers patch
    sed -i '' 's|^AVAILABILITY_PL="${SDKROOT}/${DRIVERKITROOT}|AVAILABILITY_PL="${FAKEROOT_DIR}|g' ${WORK_DIR}/xnu/bsd/sys/make_symbol_aliasing.sh
    # libsyscall patch
    sed -i '' 's|^#include.*BSD.xcconfig.*||g' ${WORK_DIR}/xnu/libsyscall/Libsyscall.xcconfig
    # xnu build patch
    sed -i '' 's|^LDFLAGS_KERNEL_SDK	= -L$(SDKROOT).*|LDFLAGS_KERNEL_SDK	= -L$(FAKEROOT_DIR)/usr/local/lib/kernel -lfirehose_kernel|g' ${WORK_DIR}/xnu/makedefs/MakeInc.def
    sed -i '' 's|^INCFLAGS_SDK	= -I$(SDKROOT)|INCFLAGS_SDK	= -I$(FAKEROOT_DIR)|g' ${WORK_DIR}/xnu/makedefs/MakeInc.def
    git apply --directory='xnu' patches/machine_routines.patch || true
}

build_dtrace() {
    if [ ! -f "${FAKEROOT_DIR}/usr/local/bin/ctfmerge" ]; then
        running "📦 Building dtrace"
        if [ ! -d "${WORK_DIR}/dtrace" ]; then
            DTRACE_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="dtrace") | .tag')
            git clone --branch ${DTRACE_VERSION} https://github.com/apple-oss-distributions/dtrace.git ${WORK_DIR}/dtrace
        fi
        SRCROOT=${WORK_DIR}/dtrace
        OBJROOT=${BUILD_DIR}/dtrace.obj
        SYMROOT=${BUILD_DIR}/dtrace.sym
        cd ${SRCROOT}
        xcodebuild install -sdk macosx -target ctfconvert -target ctfdump -target ctfmerge ARCHS="arm64" CODE_SIGN_IDENTITY="-" OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT}
        cd ${WORK_DIR}
    fi
}

build_availabilityversions() {
    if [ ! -f "${FAKEROOT_DIR}/${KERNEL_FRAMEWORK_ROOT}/Headers/AvailabilityVersions.h" ]; then
        running "📦 Building AvailabilityVersions"
        if [ ! -d "${WORK_DIR}/AvailabilityVersions" ]; then
            AVAILABILITYVERSIONS_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="AvailabilityVersions") | .tag')
            git clone --branch ${AVAILABILITYVERSIONS_VERSION} https://github.com/apple-oss-distributions/AvailabilityVersions.git ${WORK_DIR}/AvailabilityVersions
        fi
        SRCROOT=${WORK_DIR}/AvailabilityVersions
        OBJROOT=${BUILD_DIR}/
        SYMROOT=${BUILD_DIR}/
        cd ${SRCROOT}
        make install -j8 OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT}
        cd ${WORK_DIR}
    fi
}

xnu_headers() {
    if [ ! -d "${FAKEROOT_DIR}/${KERNEL_FRAMEWORK_ROOT}/PrivateHeaders" ]; then
        running "Installing xnu headers TARGET_CONFIGS=\"$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG\""
        SRCROOT=${WORK_DIR}
        OBJROOT=${BUILD_DIR}/xnu-hdrs.obj
        SYMROOT=${BUILD_DIR}/xnu-hdrs.sym
        cd ${SRCROOT}
        make installhdrs SDKROOT=macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR}
        cd ${WORK_DIR}
    fi
}

libsystem_headers() {
    if [ ! -d "${FAKEROOT_DIR}/System/Library/Frameworks/System.framework" ]; then
        running "Installing Libsystem headers"
        if [ ! -d "${WORK_DIR}/Libsystem" ]; then
            LIBSYSTEM_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="Libsystem") | .tag')
            git clone --branch ${LIBSYSTEM_VERSION} https://github.com/apple-oss-distributions/Libsystem.git ${WORK_DIR}/Libsystem
        fi
        sed -i '' 's|^#include.*BSD.xcconfig.*||g' ${WORK_DIR}/Libsystem/Libsystem.xcconfig
        SRCROOT=${WORK_DIR}/Libsystem
        OBJROOT=${BUILD_DIR}/Libsystem.obj
        SYMROOT=${BUILD_DIR}/Libsystem.sym
        cd ${SRCROOT}
        xcodebuild installhdrs -sdk macosx ARCHS="arm64 arm64e" VALID_ARCHS="arm64 arm64e" OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR}
        cd ${WORK_DIR}
    fi
}

libsyscall_headers() {
    if [ ! -f "${FAKEROOT_DIR}/usr/include/os/proc.h" ]; then
        running "Installing libsyscall headers"
        SRCROOT=${WORK_DIR}/libsyscall
        OBJROOT=${BUILD_DIR}/libsyscall.obj
        SYMROOT=${BUILD_DIR}/libsyscall.sym
        cd ${SRCROOT}
        xcodebuild installhdrs -sdk macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" ARCHS="arm64 arm64e" VALID_ARCHS="arm64 arm64e" OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR}
        cd ${WORK_DIR}
    fi
}

build_libplatform() {
    if [ ! -f "${FAKEROOT_DIR}/usr/local/include/_simple.h" ]; then
        running "📦 Building libplatform"
        if [ ! -d "${WORK_DIR}/libplatform" ]; then
            LIBPLATFORM_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="libplatform") | .tag')
            git clone --branch ${LIBPLATFORM_VERSION} https://github.com/apple-oss-distributions/libplatform.git ${WORK_DIR}/libplatform
        fi
        SRCROOT=${WORK_DIR}/libplatform
        cd ${SRCROOT}
        ditto ${SRCROOT}/include ${DSTROOT}/usr/local/include
        ditto ${SRCROOT}/private ${DSTROOT}/usr/local/include
        cd ${WORK_DIR}
    fi
}

build_libdispatch() {
    if [ ! -f "${FAKEROOT_DIR}/usr/local/lib/kernel/libfirehose_kernel.a" ]; then
        running "📦 Building libdispatch"
        if [ ! -d "${WORK_DIR}/libdispatch" ]; then
            LIBDISPATCH_VERSION=$(curl -s $RELEASE_URL | jq -r '.projects[] | select(.project=="libdispatch") | .tag')
            git clone --branch ${LIBDISPATCH_VERSION} https://github.com/apple-oss-distributions/libdispatch.git ${WORK_DIR}/libdispatch
        fi
        SRCROOT=${WORK_DIR}/libdispatch
        OBJROOT=${BUILD_DIR}/libfirehose_kernel.obj
        SYMROOT=${BUILD_DIR}/libfirehose_kernel.sym
        # libfirehose_kernel patch
        sed -i '' 's|$(SDKROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders|$(FAKEROOT_DIR)/System/Library/Frameworks/Kernel.framework/PrivateHeaders|g' ${SRCROOT}/xcodeconfig/libfirehose_kernel.xcconfig
        sed -i '' 's|$(SDKROOT)/usr/local/include|$(FAKEROOT_DIR)/usr/local/include|g' ${SRCROOT}/xcodeconfig/libfirehose_kernel.xcconfig
        cd ${SRCROOT}
        xcodebuild install -target libfirehose_kernel -sdk macosx ARCHS="arm64e" VALID_ARCHS="arm64e" OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR}
        cd ${WORK_DIR}
        mv ${FAKEROOT_DIR}/usr/local/lib/kernel/liblibfirehose_kernel.a ${FAKEROOT_DIR}/usr/local/lib/kernel/libfirehose_kernel.a
    fi
}

build_xnu() {
    if [ "$JSONDB" -ne "0" ]; then
        running "📦 Building XNU kernel with JSON compilation database"
        if [ ! -d "${KDKROOT}" ]; then
            error "KDKROOT not found: ${KDKROOT} - please install from the Developer Portal"
            exit 1
        fi
        SRCROOT=${WORK_DIR}
        OBJROOT=${BUILD_DIR}/xnu-compiledb.obj
        SYMROOT=${BUILD_DIR}/xnu-compiledb.sym
        rm -rf ${OBJROOT}
        rm -rf ${SYMROOT}
        cd ${SRCROOT}
        make SDKROOT=macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" LOGCOLORS=y BUILD_WERROR=0 BUILD_LTO=0 BUILD_JSON_COMPILATION_DATABASE=1 SRCROOT=${SRCROOT} OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR} KDKROOT=${KDKROOT} || true
        JSON_COMPILE_DB=$(find ${OBJROOT} -name compile_commands.json)
        info "JSON compilation database: ${JSON_COMPILE_DB}"
        cp -f ${JSON_COMPILE_DB} ${SRCROOT}
    else
        running "📦 Building XNU kernel TARGET_CONFIGS=\"$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG\""
        if [ ! -d "${KDKROOT}" ]; then
            error "KDKROOT not found: ${KDKROOT} - please install from the Developer Portal"
            exit 1
        fi
        SRCROOT=${WORK_DIR}
        OBJROOT=${BUILD_DIR}/xnu.obj
        SYMROOT=${BUILD_DIR}/xnu.sym
        cd ${SRCROOT}
        make install -j8 SDKROOT=macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" CONCISE=0 LOGCOLORS=y BUILD_WERROR=0 BUILD_LTO=0 SRCROOT=${SRCROOT} OBJROOT=${OBJROOT} SYMROOT=${SYMROOT} DSTROOT=${DSTROOT} FAKEROOT_DIR=${FAKEROOT_DIR} KDKROOT=${KDKROOT}
        cd ${WORK_DIR}
    fi
}

install_ipsw() {
    if ! [ -x "$(command -v ipsw)" ]; then
        running "Installing `ipsw`..."
        brew install blacktop/tap/ipsw
    fi
}

build_kc() {
    if [ -f "${BUILD_DIR}/xnu.obj/kernel.${KERNEL_CONFIG,,}.${MACHINE_CONFIG,,}" ]; then
        running "📦 Building kext collection for kernel.${KERNEL_CONFIG,,}.${MACHINE_CONFIG,,}"
        kmutil create -v -V release -a arm64e -n boot \
            -B ${BUILD_DIR}/oss-xnu.kc \
            -k ${BUILD_DIR}/xnu.obj/kernel.${KERNEL_CONFIG,,}.${MACHINE_CONFIG,,} \
            --kdk ${KDKROOT} \
            --elide-identifier com.apple.driver.SEPHibernation \
            --elide-identifier com.apple.iokit.IOSkywalkFamily \
            -r ${KDKROOT}/System/Library/Extensions \
            -r /System/Library/Extensions \
            -r /System/Library/DriverExtensions \
            -x $(ipsw kernel kmutil inspect -x --filter 'com.apple.iokit.IOSkywalkFamily|com.apple.driver.SEPHibernation') # this will skip IOSkywalkFamily and SEPHibernation (and other kexts with them as dependencies)
            # -x $(kmutil inspect -V release --no-header | grep apple | grep -v "SEPHiber\|IOSkywalkFamily" | awk '{print " -b "$1; }')
    fi
}

main() {
    if [[ "${1-}" =~ ^-*c(lean)?$ ]]; then
        running "Cleaning build directories and extra repos..."
        read -p "Are you sure? " -n 1 -r
        echo    # (optional) move to a new line
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            info "deleting ${BUILD_DIR}"
            rm -rf ${BUILD_DIR}
            info "deleting ${FAKEROOT_DIR}"
            rm -rf ${FAKEROOT_DIR}
            info "deleting ${WORK_DIR}/dtrace"
            rm -rf ${WORK_DIR}/dtrace
            info "deleting ${WORK_DIR}/AvailabilityVersions"
            rm -rf ${WORK_DIR}/AvailabilityVersions
            info "deleting ${WORK_DIR}/Libsystem"
            rm -rf ${WORK_DIR}/Libsystem
            info "deleting ${WORK_DIR}/libplatform"
            rm -rf ${WORK_DIR}/libplatform
            info "deleting ${WORK_DIR}/libdispatch"
            rm -rf ${WORK_DIR}/libdispatch
        fi
    fi
    install_deps
    choose_xnu
    get_xnu
    patches
    # venv
    # build_dtrace
    # build_availabilityversions
    # xnu_headers
    # libsystem_headers
    # libsyscall_headers
    # build_libplatform
    # build_libdispatch
    # build_xnu
    # if [[ "${1-}" =~ ^-*k(c)?$ ]]; then
    #     install_ipsw
    #     build_kc
    # fi
    echo "  🎉 XNU Build Done!"
}

main "$@"