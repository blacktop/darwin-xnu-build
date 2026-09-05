/* Userspace-only behavior needed by targets that cross a kernel/userspace boundary. */
#define UT_MODULE osfmk
#include "harness_copyio.h"
#include "mocks/mock_dynamic.h"
#include <kern/zalloc.h>
#include <kern/zalloc_internal.h>
#include <sanitizer/asan_interface.h>
#include <sys/errno.h>

T_MOCK_DECLARE(void, zfree_ext,
    (zone_t zone, zone_stats_t stats, void *address, uint64_t combined_size));
T_MOCK_DECLARE(struct kalloc_result, zalloc_ext,
    (zone_t zone, zone_stats_t stats, zalloc_flags_t flags));

extern void *harness_low_alloc(unsigned long size);

#define HARNESS_VOUCHER_SLOTS 128

static struct {
	uint8_t *hvp_base;
	size_t hvp_element_size;
	unsigned hvp_next;
	unsigned hvp_free_count;
	void *hvp_free[HARNESS_VOUCHER_SLOTS];
	bool hvp_in_use[HARNESS_VOUCHER_SLOTS];
} harness_voucher_pool;

static struct harness_copyio *active_copyio;

void
harness_copyio_begin(struct harness_copyio *copyio, user_addr_t base, void *buffer, size_t size)
{
	copyio->hc_base = base;
	copyio->hc_buffer = buffer;
	copyio->hc_size = size;
	copyio->hc_previous = active_copyio;
	active_copyio = copyio;
}

void
harness_copyio_end(struct harness_copyio *copyio)
{
	if (active_copyio != copyio) {
		raw_printf("FUZZ BUG: copyio regions ended out of order\n");
		abort();
	}
	active_copyio = copyio->hc_previous;
}

static bool
harness_copyio_offset(user_addr_t address, size_t length, size_t *offset)
{
	if (active_copyio == NULL || address < active_copyio->hc_base ||
	    length > active_copyio->hc_size ||
	    (length > 0 && active_copyio->hc_buffer == NULL)) {
		return false;
	}

	*offset = (size_t)(address - active_copyio->hc_base);
	return *offset <= active_copyio->hc_size - length;
}

#ifdef copyin
#undef copyin
#endif
#ifdef copyout
#undef copyout
#endif
T_MOCK_DECLARE(int, copyin, (const user_addr_t address, void *kernel, size_t length));
T_MOCK_DECLARE(int, copyout, (const void *kernel, user_addr_t address, size_t length));

T_MOCK_SET_PERM_FUNC(int, copyin,
    (const user_addr_t address, void *kernel, size_t length))
{
	size_t offset;

	if (active_copyio == NULL) {
		return 0;
	}
	if (!harness_copyio_offset(address, length, &offset)) {
		return EFAULT;
	}
	if (length > 0) {
		memcpy(kernel, &active_copyio->hc_buffer[offset], length);
	}
	return 0;
}

T_MOCK_SET_PERM_FUNC(int, copyout,
    (const void *kernel, user_addr_t address, size_t length))
{
	size_t offset;

	if (active_copyio == NULL) {
		return 0;
	}
	if (!harness_copyio_offset(address, length, &offset)) {
		return EFAULT;
	}
	if (length > 0) {
		memcpy(&active_copyio->hc_buffer[offset], kernel, length);
	}
	return 0;
}

/* SMR links cannot encode ASan's high heap addresses; use serial, reusable low storage. */
T_MOCK_SET_PERM_FUNC(struct kalloc_result, zalloc_ext,
    (zone_t zone, zone_stats_t stats, zalloc_flags_t flags))
{
	if (zone != zone_by_id(ZONE_ID_IPC_VOUCHERS)) {
		return T_MOCK_DEFAULT_ACTION(zalloc_ext)(zone, stats, flags);
	}

	if (harness_voucher_pool.hvp_base == NULL) {
		harness_voucher_pool.hvp_element_size = zone->z_elem_size;
		harness_voucher_pool.hvp_base = harness_low_alloc(
		    zone->z_elem_size * HARNESS_VOUCHER_SLOTS);
		ASAN_POISON_MEMORY_REGION(harness_voucher_pool.hvp_base,
		    zone->z_elem_size * HARNESS_VOUCHER_SLOTS);
	}
	void *address;
	if (harness_voucher_pool.hvp_free_count > 0) {
		address = harness_voucher_pool.hvp_free[--harness_voucher_pool.hvp_free_count];
	} else {
		if (harness_voucher_pool.hvp_next == HARNESS_VOUCHER_SLOTS) {
			raw_printf("FUZZ BUG: voucher pool exhausted\n");
			abort();
		}
		address = &harness_voucher_pool.hvp_base[
		    harness_voucher_pool.hvp_next++ * harness_voucher_pool.hvp_element_size];
	}
	size_t index = ((uint8_t *)address - harness_voucher_pool.hvp_base) /
	    harness_voucher_pool.hvp_element_size;
	if (harness_voucher_pool.hvp_in_use[index]) {
		raw_printf("FUZZ BUG: voucher pool allocated a live object\n");
		abort();
	}
	harness_voucher_pool.hvp_in_use[index] = true;
	ASAN_UNPOISON_MEMORY_REGION(address, harness_voucher_pool.hvp_element_size);
	bzero(address, harness_voucher_pool.hvp_element_size);
	return (struct kalloc_result){ .addr = address, .size = harness_voucher_pool.hvp_element_size };
}

/* The harness is serial, so removal from the SMR hash ends the only possible read section. */
#ifdef zfree_id_smr
#undef zfree_id_smr
#endif
T_MOCK_F(void, zfree_id_smr, (zone_id_t zid, void *address), (zid, address))
{
	if (zid == ZONE_ID_IPC_VOUCHERS) {
		uintptr_t base = (uintptr_t)harness_voucher_pool.hvp_base;
		uintptr_t value = (uintptr_t)address;
		size_t pool_size = harness_voucher_pool.hvp_element_size * HARNESS_VOUCHER_SLOTS;
		if (value < base || value - base >= pool_size ||
		    (value - base) % harness_voucher_pool.hvp_element_size != 0) {
			raw_printf("FUZZ BUG: voucher pool received an unknown object\n");
			abort();
		}
		size_t index = (value - base) / harness_voucher_pool.hvp_element_size;
		if (!harness_voucher_pool.hvp_in_use[index]) {
			raw_printf("FUZZ BUG: voucher pool double free\n");
			abort();
	}
	harness_voucher_pool.hvp_in_use[index] = false;
	ASAN_POISON_MEMORY_REGION(address, harness_voucher_pool.hvp_element_size);
	harness_voucher_pool.hvp_free[harness_voucher_pool.hvp_free_count++] = address;
		return;
	}
	T_MOCK_MOCK(zfree_ext)(zone_by_id(zid), NULL, address, 0);
}
