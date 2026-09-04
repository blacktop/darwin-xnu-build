/*
 * Startup-plan customization, through Apple's own hook (mocks/fake_kinit.h).
 *
 * fake_kinit() builds a default plan, calls this hook, then runs the plan. The weak definition in
 * fake_kinit.c does nothing; a strong definition in the test executable overrides it. This is the
 * supported way to adjust the bootstrap, so nothing in xnu/tests/unit has to be patched.
 */
#define UT_MODULE osfmk
#include "mocks/fake_kinit.h"
#include "mocks/osfmk/unit_test_utils.h"

FAKE_KINIT_CUSTOMIZE_PLAN()
{
	/*
	 * osfmk/arm64/sop.c registers sop_page_pool_init at STARTUP(ZALLOC, STARTUP_RANK_LAST). It
	 * grabs pages with vm_page_grab_options() and asserts each one has a non-zero physical address
	 * inside [vm_first_phys, vm_last_phys). A userspace process has no physical memory and the VM
	 * page layer is mocked, so the assert always fires. Nothing downstream of the pool is reachable
	 * here: it exists to hand redzone stack pages to the exception path.
	 *
	 * The list replaces the subsystem's skip set rather than adding to it, so Apple's own
	 * kauth_cred_init entry (SMR is unsupported in user mode) has to be repeated.
	 */
	/*
	 * The array must be static: _fki_plan_set_startup_skip_func_names only stores the pointer, and
	 * the plan is run after this hook returns. The fki_plan_set_startup_skip_func_names macro wraps
	 * its arguments in a compound literal, which has automatic storage here and would dangle, so
	 * call the underlying function with storage that outlives the hook.
	 */
	static const char *skip_at_zalloc[] = {
		"kauth_cred_init",
		"sop_page_pool_init",
		NULL,
	};

	_fki_plan_set_startup_skip_func_names(STARTUP_SUB_ZALLOC, skip_at_zalloc);
}
