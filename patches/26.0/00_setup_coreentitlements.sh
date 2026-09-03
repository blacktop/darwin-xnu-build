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

# Copy Context.h and API.h from KDK (these exist in KDK)
if [ -f "${KDK_CE_PATH}/V2/Context.h" ]; then
    echo "  Copying Context.h from KDK..."
    cp "${KDK_CE_PATH}/V2/Context.h" "${EXTERNAL_HEADERS}/CoreEntitlements/V2/"
fi

if [ -f "${KDK_CE_PATH}/V2/API.h" ]; then
    echo "  Copying API.h from KDK..."
    cp "${KDK_CE_PATH}/V2/API.h" "${EXTERNAL_HEADERS}/CoreEntitlements/V2/"
fi

if [ -f "${KDK_CE_PATH}/V2/Return.h" ]; then
    echo "  Copying Return.h from KDK..."
    cp "${KDK_CE_PATH}/V2/Return.h" "${EXTERNAL_HEADERS}/CoreEntitlements/V2/"
fi

# Create minimal Kernel.h stub (not in KDK, needed by amfi.h)
echo "  Creating Kernel.h stub..."
cat >"${EXTERNAL_HEADERS}/CoreEntitlements/V2/Kernel.h" <<'EOF'
#ifndef CORE_ENTITLEMENTS_V2_KERNEL_H
#define CORE_ENTITLEMENTS_V2_KERNEL_H

#include <stdbool.h>
#include <stdint.h>
#include <CoreEntitlements/CoreEntitlements.h>
#include <CoreEntitlements/der_vm.h>

struct CEQueryContext;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct coreentitlements_kernel_api {
    uint32_t version;
    CEError_t kNoError;
    CEError_t kMalformedEntitlements;
    CEError_t kNotEligibleForAcceleration;

    const char *(*GetErrorString)(CEError_t error);

    CEError_t (*ContextQuery)(CEQueryContext_t ctx,
        const CEQueryOperation_t *__counted_by(queryLength) query,
        size_t queryLength);

    CEError_t (*Validate)(const CERuntime_t rt,
        CEValidationResult *result,
        const uint8_t *__ended_by(blob_end) blob,
        const uint8_t *blob_end);

    CEError_t (*AcquireUnmanagedContext)(const CERuntime_t rt,
        CEValidationResult validationResult,
        struct CEQueryContext *ctx);

    der_vm_context_t (*der_vm_context_create)(const CERuntime_t rt,
        ccder_tag dictionary_tag,
        bool sorted_keys,
        const uint8_t *__ended_by(der_end) der,
        const uint8_t *der_end);

    der_vm_context_t (*der_vm_execute)(der_vm_context_t context,
        CEQueryOperation_t op);

    der_vm_context_t (*der_vm_execute_seq)(der_vm_context_t context,
        const CEQueryOperation_t *__counted_by(queryLength) query,
        size_t queryLength);

    bool (*der_vm_context_is_valid)(der_vm_context_t context);
    bool (*der_vm_bool_from_context)(der_vm_context_t context);

    CEError_t (*IndexSizeForContext)(CEQueryContext_t ctx, size_t *size);
    CEError_t (*BuildIndexForContext)(CEQueryContext_t ctx);
    bool (*ContextIsAccelerated)(CEQueryContext_t ctx);
} coreentitlements_kernel_api;

typedef struct coreentitlements_kernel_api CEKernelAPI_t;

#ifdef __cplusplus
}
#endif

#endif /* CORE_ENTITLEMENTS_V2_KERNEL_H */
EOF

# Older revisions of this script wrote a hand-made os/firehose_buffer_private.h here. The real
# header is installed by libdispatch into ${FAKEROOT_DIR}/usr/local/include/kernel, and a stale
# stub under EXTERNAL_HEADERS would shadow it, so remove any leftover copy.
if [ -f "${EXTERNAL_HEADERS}/os/firehose_buffer_private.h" ]; then
    echo "  Removing obsolete firehose_buffer_private.h stub..."
    rm -f "${EXTERNAL_HEADERS}/os/firehose_buffer_private.h"
    rmdir "${EXTERNAL_HEADERS}/os" 2>/dev/null || true
fi

echo "CoreEntitlements V2 setup complete."
