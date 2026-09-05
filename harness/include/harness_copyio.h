#ifndef HARNESS_COPYIO_H
#define HARNESS_COPYIO_H

#include "mocks/std_safe.h"
#include <mach/vm_types.h>

struct harness_copyio {
	user_addr_t hc_base;
	uint8_t *hc_buffer;
	size_t hc_size;
	struct harness_copyio *hc_previous;
};

void harness_copyio_begin(struct harness_copyio *copyio, user_addr_t base,
    void *buffer, size_t size);
void harness_copyio_end(struct harness_copyio *copyio);

#endif /* HARNESS_COPYIO_H */
