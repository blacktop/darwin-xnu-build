#!/bin/bash

# Setup CoreEntitlements V2 headers from KDK
# This script creates the necessary directory structure and copies KDK headers

EXTERNAL_HEADERS="./EXTERNAL_HEADERS"
# Prefer the KDKROOT provided by build.sh, falling back to the known 26.1 then 26.0 KDKs.
KDKROOT_CLEAN="${KDKROOT%/}"
if [ -z "${KDKROOT_CLEAN}" ]; then
    KDKROOT_CLEAN="/Library/Developer/KDKs/KDK_26.1_25B5062e.kdk"
fi
KDK_CE_PATH="${KDKROOT_CLEAN}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements"
if [ ! -d "${KDK_CE_PATH}" ]; then
    KDK_CE_PATH="/Library/Developer/KDKs/KDK_26.0_25A353.kdk/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/platform/CoreEntitlements"
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
cat > "${EXTERNAL_HEADERS}/CoreEntitlements/V2/Kernel.h" <<'EOF'
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

# Create os directory for firehose header if needed
mkdir -p "${EXTERNAL_HEADERS}/os"

# Create firehose_buffer_private.h stub
echo "  Creating firehose_buffer_private.h stub..."
cat > "${EXTERNAL_HEADERS}/os/firehose_buffer_private.h" <<'EOF'
#ifndef _OS_FIREHOSE_BUFFER_PRIVATE_H_
#define _OS_FIREHOSE_BUFFER_PRIVATE_H_

#include <stdbool.h>
#include <stdint.h>
#include <mach/vm_types.h>
#include <firehose/firehose_types_private.h>

struct firehose_buffer_range_s {
    uint16_t fbr_offset;
    uint16_t fbr_length;
};

#define FIREHOSE_BUFFER_KERNEL_CHUNK_COUNT 1
#define FIREHOSE_BUFFER_KERNEL_DEFAULT_CHUNK_COUNT 64
#define FIREHOSE_BUFFER_KERNEL_DEFAULT_IO_PAGES 0

__BEGIN_DECLS

firehose_tracepoint_t __firehose_buffer_tracepoint_reserve(uint64_t timestamp,
    firehose_stream_t stream,
    uint16_t pub_size,
    uint16_t priv_size,
    uint8_t **priv_data_out);

void __firehose_buffer_tracepoint_flush(firehose_tracepoint_t ft,
    firehose_tracepoint_id_u ftid);

void __firehose_buffer_push_to_logd(firehose_buffer_t fb, bool for_io);
void __firehose_allocate(vm_offset_t *addr, vm_size_t size);
void __firehose_critical_region_enter(void);
void __firehose_critical_region_leave(void);

bool __firehose_kernel_configuration_valid(uint32_t chunk_count, uint32_t io_pages);
firehose_buffer_t __firehose_buffer_create(size_t *size);
bool __firehose_merge_updates(firehose_push_reply_t reply);

__END_DECLS

#endif /* _OS_FIREHOSE_BUFFER_PRIVATE_H */
EOF

echo "CoreEntitlements V2 setup complete."
