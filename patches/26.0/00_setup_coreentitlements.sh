#!/bin/bash

set -euo pipefail

# Setup CoreEntitlements V2 headers from KDK
# This script creates the necessary directory structure and copies KDK headers

EXTERNAL_HEADERS="./EXTERNAL_HEADERS"
# Prefer the KDKROOT provided by build.sh, falling back to known 26.x KDKs.
KDKROOT_CLEAN="${KDKROOT:-}"
KDKROOT_CLEAN="${KDKROOT_CLEAN%/}"
KDK_CE_PATH=""

# Try KDKROOT from environment first
if [ -n "${KDKROOT_CLEAN}" ] && [ -d "${KDKROOT_CLEAN}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements" ]; then
    KDK_CE_PATH="${KDKROOT_CLEAN}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements"
fi

# Fallback: try known KDKs in reverse version order
if [ -z "${KDK_CE_PATH}" ]; then
    for KDK in /Library/Developer/KDKs/KDK_26.*.kdk; do
        if [ -d "${KDK}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements" ]; then
            KDK_CE_PATH="${KDK}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements"
        fi
    done
fi

if [ -z "${KDK_CE_PATH}" ]; then
    echo "ERROR: No suitable KDK found for CoreEntitlements headers"
    exit 1
fi

echo "Setting up CoreEntitlements V2 headers..."

# Create V2 directory if it doesn't exist
mkdir -p "${EXTERNAL_HEADERS}/CoreEntitlements/V2"

# Copy every V2 header (API.h pulls in Closure.h, Acceleration.h and StackProtector.h).
echo "  Copying V2 headers from ${KDK_CE_PATH}..."
cp "${KDK_CE_PATH}/V2/"*.h "${EXTERNAL_HEADERS}/CoreEntitlements/V2/"

# Kernel.h (the CEKernelAPI_t function table AMFI hands to xnu) is not under PrivateHeaders; KDKs from
# 26.4 on ship it in System/Library/KernelSupport. Earlier revisions of this script wrote a made-up
# struct here, which compiled for VMAPPLE only because no code path used the table, and broke every
# CODE_SIGNING_MONITOR config. Use Apple's header, borrowing it from another installed 26.x KDK when
# the target KDK predates it (the layout is identical across 26.4 through 26.6.2).
KERNEL_H_REL="System/Library/KernelSupport/CoreEntitlements/V2/Kernel.h"
KERNEL_H=""
if [ -n "${KDKROOT_CLEAN}" ] && [ -f "${KDKROOT_CLEAN}/${KERNEL_H_REL}" ]; then
    KERNEL_H="${KDKROOT_CLEAN}/${KERNEL_H_REL}"
else
    for KDK in /Library/Developer/KDKs/KDK_26.*.kdk; do
        if [ -f "${KDK}/${KERNEL_H_REL}" ]; then
            KERNEL_H="${KDK}/${KERNEL_H_REL}"
        fi
    done
    if [ -n "${KERNEL_H}" ]; then
        echo "  WARNING: ${KDKROOT_CLEAN:-the selected KDK} has no ${KERNEL_H_REL}; borrowing ${KERNEL_H}"
    fi
fi
if [ -z "${KERNEL_H}" ]; then
    echo "ERROR: no installed 26.x KDK provides ${KERNEL_H_REL}; install KDK 26.4 or newer"
    exit 1
fi
echo "  Copying Kernel.h from ${KERNEL_H}..."
cp "${KERNEL_H}" "${EXTERNAL_HEADERS}/CoreEntitlements/V2/Kernel.h"

# Older revisions of this script wrote a hand-made os/firehose_buffer_private.h here. The real
# header is installed by libdispatch into ${FAKEROOT_DIR}/usr/local/include/kernel, and a stale
# stub under EXTERNAL_HEADERS would shadow it, so remove any leftover copy.
if [ -f "${EXTERNAL_HEADERS}/os/firehose_buffer_private.h" ]; then
    echo "  Removing obsolete firehose_buffer_private.h stub..."
    rm -f "${EXTERNAL_HEADERS}/os/firehose_buffer_private.h"
    rmdir "${EXTERNAL_HEADERS}/os" 2>/dev/null || true
fi

echo "CoreEntitlements V2 setup complete."
