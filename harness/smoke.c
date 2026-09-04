/*
 * Smoke test: did Apple's fake_kinit() constructor bring the userspace kernel up?
 *
 * Built like one of Apple's unit tests (kernel flags, no libc headers) and linked against
 * libkernel.<config>.<machine>.dylib and libmocks.dylib. fake_kinit() runs before main().
 */
#define UT_MODULE osfmk
#include "mocks/osfmk/unit_test_utils.h"
#include <kern/startup.h>

int
main(void)
{
	raw_printf("startup_phase=%u (STARTUP_SUB_SYSCTL=%u, STARTUP_SUB_EARLY_BOOT=%u)\n",
	    (unsigned)startup_phase, (unsigned)STARTUP_SUB_SYSCTL, (unsigned)STARTUP_SUB_EARLY_BOOT);
	if (startup_phase < STARTUP_SUB_SYSCTL) {
		raw_printf("FAIL: fake_kinit() stopped before STARTUP_SUB_SYSCTL\n");
		return 1;
	}
	raw_printf("PASS: userspace kernel bootstrapped\n");
	return 0;
}
