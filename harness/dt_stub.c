/*
 * darwintest replacement for the public toolchain.
 *
 * Apple links libdarwintest.a into every test executable and routes assertions raised inside the
 * kernel and mocks dylibs through the dt_proxy callbacks (xnu/tests/unit/mocks/dt_proxy.c).
 * libdarwintest is not public, so this file provides the same callbacks on top of raw_printf() and
 * abort(). It is compiled with the kernel's recorded flags (no libc headers), like dt_proxy.c.
 */
#include "mocks/std_safe.h"
#include "mocks/dt_proxy.h"
#include "mocks/osfmk/unit_test_utils.h"

static void
st_assert_true(bool cond, const char *msg)
{
	if (!cond) {
		raw_printf("FAIL: %s\n", msg);
		abort();
	}
}

static void
st_assert_notnull(void *ptr, const char *msg)
{
	st_assert_true(ptr != NULL, msg);
}

static void
st_assert_posix_zero(int v, const char *msg)
{
	if (v != 0) {
		raw_printf("FAIL: %s (posix error %d)\n", msg, v);
		abort();
	}
}

static void
st_assert_mach_success(kern_return_t kr, const char *msg)
{
	if (kr != KERN_SUCCESS) {
		raw_printf("FAIL: %s (kern_return_t %d)\n", msg, kr);
		abort();
	}
}

static void
st_log(const char *msg)
{
	raw_printf("%s\n", msg);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
static void
st_log_fmtstr(const char *fmt, const char *msg)
{
	raw_printf(fmt, msg);
	raw_printf("\n");
}
#pragma clang diagnostic pop

static void
st_fail(const char *msg)
{
	raw_printf("FAIL: %s\n", msg);
	abort();
}

static void
st_quiet(void)
{
}

static bool
st_state_pass(void)
{
	return true;
}

static struct dt_proxy_callbacks st_callbacks = {
	.t_assert_true = &st_assert_true,
	.t_assert_notnull = &st_assert_notnull,
	.t_assert_posix_zero = &st_assert_posix_zero,
	.t_assert_mach_success = &st_assert_mach_success,
	.t_log = &st_log,
	.t_log_fmtstr = &st_log_fmtstr,
	.t_fail = &st_fail,
	.t_quiet = &st_quiet,
	.t_state_pass = &st_state_pass,
};

/* Runs after fake_kinit()'s constructor, exactly like Apple's dt_init(). */
__attribute__((constructor)) void
dt_init(void)
{
	set_dt_proxy_attached(&st_callbacks);
	set_dt_proxy_mock(&st_callbacks);
}
