/*
 * Minimal darwintest replacement.
 *
 * Apple's tests under xnu/tests/unit include <darwintest.h> and link libdarwintest.a, neither of
 * which ships in the public SDK. This header implements the macro surface those tests actually use
 * so they compile and run unmodified; harness/dt_runner.c provides the main() that runs them.
 *
 * Deliberately not a reimplementation of darwintest: no per-test child processes, no metadata
 * handling, no leak checking. Each T_DECL becomes a function registered by a constructor, run in
 * order in one process, and an assertion failure aborts, so a failure is never mistaken for a pass.
 * Test files are compiled with kernel flags (-nostdlibinc), so nothing here may include libc.
 */
#ifndef HARNESS_DARWINTEST_H
#define HARNESS_DARWINTEST_H

#include "mocks/std_safe.h"
#include "mocks/osfmk/unit_test_utils.h"

__BEGIN_DECLS

struct harness_test {
	const char *ht_name;
	const char *ht_desc;
	void (*ht_func)(void);
};

extern void harness_register_test(const struct harness_test *test);
extern void harness_fail(const char *file, int line, const char *expr, const char *fmt, ...)
__attribute__((format(printf, 4, 5), noreturn));
extern void harness_pass(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void harness_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

__END_DECLS

#define T_DECL(name, desc, ...)                                                 \
	static void harness_test_##name(void);                                  \
	static const struct harness_test harness_test_desc_##name = {           \
	        .ht_name = #name, .ht_desc = desc, .ht_func = harness_test_##name, \
	};                                                                      \
	__attribute__((constructor)) static void                                \
	harness_register_##name(void)                                           \
	{                                                                       \
	        harness_register_test(&harness_test_desc_##name);               \
	}                                                                       \
	static void harness_test_##name(void)

/* Metadata carries no behavior here. */
#define T_GLOBAL_META(...)
#define T_META_NAMESPACE(...)
#define T_META_RUN_CONCURRENTLY(...)
#define T_META_RADAR_COMPONENT_NAME(...)
#define T_META_RADAR_COMPONENT_VERSION(...)
#define T_META_TAG_VM_PREFERRED
#define T_META_TIMEOUT(...)
#define T_META_OWNER(...)
#define T_META_EXPECTFAIL

/*
 * In darwintest T_QUIET suppresses the log line of a passing assertion. Assertions here are silent
 * on success already, so it only has to be a valid statement.
 */
#define T_QUIET do { } while (0)

#define T_SETUPBEGIN do { } while (0)
#define T_SETUPEND do { } while (0)

#define harness_check(cond, expr, ...)                                          \
	do {                                                                    \
	        if (!(cond)) {                                                  \
	                harness_fail(__FILE__, __LINE__, expr, __VA_ARGS__);    \
	        }                                                               \
	} while (0)

#define T_ASSERT_TRUE(expr, ...)        harness_check((expr), #expr " is true", __VA_ARGS__)
#define T_ASSERT_FALSE(expr, ...)       harness_check(!(expr), #expr " is false", __VA_ARGS__)
#define T_ASSERT_NOTNULL(p, ...)        harness_check((p) != NULL, #p " != NULL", __VA_ARGS__)
#define T_ASSERT_NULL(p, ...)           harness_check((p) == NULL, #p " == NULL", __VA_ARGS__)
#define T_ASSERT_EQ(a, b, ...)          harness_check((a) == (b), #a " == " #b, __VA_ARGS__)
#define T_ASSERT_NE(a, b, ...)          harness_check((a) != (b), #a " != " #b, __VA_ARGS__)
#define T_ASSERT_GT(a, b, ...)          harness_check((a) > (b), #a " > " #b, __VA_ARGS__)
#define T_ASSERT_GE(a, b, ...)          harness_check((a) >= (b), #a " >= " #b, __VA_ARGS__)
#define T_ASSERT_LT(a, b, ...)          harness_check((a) < (b), #a " < " #b, __VA_ARGS__)
#define T_ASSERT_LE(a, b, ...)          harness_check((a) <= (b), #a " <= " #b, __VA_ARGS__)
#define T_ASSERT_EQ_PTR(a, b, ...)      harness_check((void *)(a) == (void *)(b), #a " == " #b, __VA_ARGS__)
#define T_ASSERT_NE_PTR(a, b, ...)      harness_check((void *)(a) != (void *)(b), #a " != " #b, __VA_ARGS__)
#define T_ASSERT_EQ_INT(a, b, ...)      T_ASSERT_EQ(a, b, __VA_ARGS__)
#define T_ASSERT_NE_INT(a, b, ...)      T_ASSERT_NE(a, b, __VA_ARGS__)
#define T_ASSERT_EQ_UINT(a, b, ...)     T_ASSERT_EQ(a, b, __VA_ARGS__)
#define T_ASSERT_EQ_ULLONG(a, b, ...)   T_ASSERT_EQ(a, b, __VA_ARGS__)
#define T_ASSERT_MACH_SUCCESS(kr, ...)  harness_check((kr) == KERN_SUCCESS, #kr " == KERN_SUCCESS", __VA_ARGS__)
#define T_ASSERT_MACH_ERROR(err, kr, ...) harness_check((kr) == (err), #kr " == " #err, __VA_ARGS__)
#define T_ASSERT_POSIX_ZERO(v, ...)     harness_check((v) == 0, #v " == 0", __VA_ARGS__)
#define T_ASSERT_FAIL(...)              harness_fail(__FILE__, __LINE__, "T_ASSERT_FAIL", __VA_ARGS__)

/*
 * darwintest distinguishes EXPECT (record and continue) from ASSERT (stop the test). Treating both
 * as fatal is the conservative choice: it can only turn a silent failure into a loud one.
 */
#define T_EXPECT_TRUE(expr, ...)        T_ASSERT_TRUE(expr, __VA_ARGS__)
#define T_EXPECT_FALSE(expr, ...)       T_ASSERT_FALSE(expr, __VA_ARGS__)
#define T_EXPECT_EQ(a, b, ...)          T_ASSERT_EQ(a, b, __VA_ARGS__)
#define T_EXPECT_NE(a, b, ...)          T_ASSERT_NE(a, b, __VA_ARGS__)
#define T_EXPECT_LE(a, b, ...)          T_ASSERT_LE(a, b, __VA_ARGS__)
#define T_EXPECT_GT(a, b, ...)          T_ASSERT_GT(a, b, __VA_ARGS__)
#define T_EXPECT_EQ_PTR(a, b, ...)      T_ASSERT_EQ_PTR(a, b, __VA_ARGS__)
#define T_EXPECT_NE_PTR(a, b, ...)      T_ASSERT_NE_PTR(a, b, __VA_ARGS__)
#define T_EXPECT_NOTNULL(p, ...)        T_ASSERT_NOTNULL(p, __VA_ARGS__)
#define T_EXPECT_NULL(p, ...)           T_ASSERT_NULL(p, __VA_ARGS__)
#define T_EXPECT_MACH_SUCCESS(kr, ...)  T_ASSERT_MACH_SUCCESS(kr, __VA_ARGS__)
#define T_EXPECT_MACH_ERROR(err, kr, ...) T_ASSERT_MACH_ERROR(err, kr, __VA_ARGS__)

#define T_PASS(...)                     harness_pass(__VA_ARGS__)
#define T_FAIL(...)                     harness_fail(__FILE__, __LINE__, "T_FAIL", __VA_ARGS__)
#define T_LOG(...)                      harness_log(__VA_ARGS__)
#define T_SKIP(...)                     harness_log(__VA_ARGS__)

#endif /* HARNESS_DARWINTEST_H */
