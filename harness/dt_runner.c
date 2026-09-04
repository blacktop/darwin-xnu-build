/*
 * Runner for tests written against harness/include/darwintest.h.
 *
 * Each T_DECL registers itself from a constructor. Those run before main(), and so does
 * mocks/fake_kinit.c's constructor, so the kernel is already bootstrapped by the time tests run.
 * Tests execute in registration order in one process; the first failing assertion aborts, so a
 * crash or a hang can never be reported as success.
 */
#define UT_MODULE osfmk
#include "mocks/std_safe.h"
#include "mocks/osfmk/unit_test_utils.h"
#include <kern/startup.h>
#include "darwintest.h"

#define HARNESS_MAX_TESTS 128

static const struct harness_test *harness_tests[HARNESS_MAX_TESTS];
static unsigned harness_test_count;
static const char *harness_current;

void
harness_register_test(const struct harness_test *test)
{
	if (harness_test_count >= HARNESS_MAX_TESTS) {
		raw_printf("harness: too many tests registered\n");
		abort();
	}
	harness_tests[harness_test_count++] = test;
}

void
harness_fail(const char *file, int line, const char *expr, const char *fmt, ...)
{
	char msg[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	raw_printf("FAIL %s: %s (%s) at %s:%d\n", harness_current ? harness_current : "<init>",
	    msg, expr, file, line);
	abort();
}

void
harness_pass(const char *fmt, ...)
{
	char msg[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	raw_printf("  pass: %s\n", msg);
}

void
harness_log(const char *fmt, ...)
{
	char msg[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	raw_printf("  log: %s\n", msg);
}

int
main(void)
{
	raw_printf("startup_phase=%u, %u test(s)\n", (unsigned)startup_phase, harness_test_count);
	if (harness_test_count == 0) {
		raw_printf("FAIL: no tests registered\n");
		return 1;
	}

	for (unsigned i = 0; i < harness_test_count; i++) {
		harness_current = harness_tests[i]->ht_name;
		raw_printf("[%u/%u] %s\n", i + 1, harness_test_count, harness_current);
		harness_tests[i]->ht_func();
	}
	harness_current = NULL;

	raw_printf("PASS: %u test(s)\n", harness_test_count);
	return 0;
}
