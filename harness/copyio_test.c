/* Focused checks for the logical userspace buffer used by parser fuzz targets. */
#define UT_MODULE osfmk
#include <darwintest.h>
#include "harness_copyio.h"

#ifdef copyin
#undef copyin
#endif
#ifdef copyout
#undef copyout
#endif
extern int copyin(const user_addr_t address, void *kernel, size_t length);
extern int copyout(const void *kernel, user_addr_t address, size_t length);

T_DECL(copyio_buffer_round_trip, "copyin and copyout transfer bytes inside the active buffer")
{
	struct harness_copyio copyio;
	uint8_t user[] = { 1, 2, 3, 4 };
	uint8_t kernel[2] = {};
	const uint8_t result[] = { 9, 8 };

	harness_copyio_begin(&copyio, 0x1000, user, sizeof(user));
	T_ASSERT_POSIX_ZERO(copyin(0x1001, kernel, sizeof(kernel)), "copyin succeeds");
	T_ASSERT_EQ(kernel[0], 2, "copyin copied first byte");
	T_ASSERT_EQ(kernel[1], 3, "copyin copied second byte");
	T_ASSERT_POSIX_ZERO(copyout(result, 0x1002, sizeof(result)), "copyout succeeds");
	T_ASSERT_EQ(user[2], 9, "copyout copied first byte");
	T_ASSERT_EQ(user[3], 8, "copyout copied second byte");
	T_ASSERT_NE(copyin(0x0fff, kernel, 1), 0, "copyin rejects an address below the buffer");
	T_ASSERT_NE(copyout(result, 0x1004, 1), 0, "copyout rejects the end address");
	harness_copyio_end(&copyio);
}
