/* Fuzz target: user voucher recipes, trap copyin/copyout, and teardown. */
#define UT_MODULE osfmk
#include "harness_copyio.h"
#include "mocks/osfmk/unit_test_utils.h"
#include <ipc/ipc_policy.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/host.h>
#include <kern/ipc_kobject.h>
#include <kern/task.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/mach_voucher_types.h>

#define VOUCHER_USER_BASE 0x10000000ULL
#define VOUCHER_RECIPE_OFFSET 16
#define VOUCHER_BUFFER_SIZE 512

static const ipc_space_policy_t voucher_policies[] = {
	IPC_SPACE_POLICY_DEFAULT,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V0,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V1,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V2,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V3,
	IPC_SPACE_POLICY_DEFAULT | IPC_SPACE_POLICY_PLATFORM | IPC_POLICY_ENHANCED_V3,
	IPC_SPACE_POLICY_DEFAULT | IPC_SPACE_POLICY_CONTAINED,
};

static mach_voucher_attr_recipe_command_t
fuzz_voucher_command(uint8_t selector)
{
	static const mach_voucher_attr_recipe_command_t commands[] = {
		MACH_VOUCHER_ATTR_USER_DATA_STORE,
		MACH_VOUCHER_ATTR_USER_DATA_STORE,
		MACH_VOUCHER_ATTR_NOOP,
		MACH_VOUCHER_ATTR_COPY,
		MACH_VOUCHER_ATTR_REMOVE,
		MACH_VOUCHER_ATTR_REDEEM,
		MACH_VOUCHER_ATTR_UNIT_TEST_VECTOR_ALLOWED,
		MACH_VOUCHER_ATTR_UNIT_TEST_VECTOR_DISALLOWED,
	};

	if (selector & 0x80) {
		return selector;
	}
	return commands[selector % (sizeof(commands) / sizeof(commands[0]))];
}

static mach_port_name_t
fuzz_host_name(void)
{
	static mach_port_name_t name;

	if (!MACH_PORT_VALID(name)) {
		ipc_port_t port = ipc_kobject_alloc_port((ipc_kobject_t)host_self(), IKOT_HOST,
		    IPC_KOBJECT_ALLOC_MAKE_SEND);
		kern_return_t kr = ipc_object_copyout(current_space(), port,
		    MACH_MSG_TYPE_PORT_SEND, IPC_OBJECT_COPYOUT_FLAGS_NONE, NULL, &name);
		if (kr != KERN_SUCCESS || !MACH_PORT_VALID(name)) {
			raw_printf("FUZZ BUG: host-name setup failed\n");
			abort();
		}
	}
	return name;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t user[VOUCHER_BUFFER_SIZE] = {};
	struct harness_copyio copyio;
	ipc_space_policy_t old_policy = ipc_space_policy(current_space());
	size_t content_size = size < VOUCHER_BUFFER_SIZE - VOUCHER_RECIPE_OFFSET -
	    sizeof(mach_voucher_attr_recipe_data_t) ? size :
	    VOUCHER_BUFFER_SIZE - VOUCHER_RECIPE_OFFSET - sizeof(mach_voucher_attr_recipe_data_t);
	mach_voucher_attr_recipe_t recipe =
	    (mach_voucher_attr_recipe_t)(void *)&user[VOUCHER_RECIPE_OFFSET];
	mach_voucher_attr_recipe_command_t command;

	if (size == 0) {
		return 0;
	}

	recipe->key = MACH_VOUCHER_ATTR_KEY_USER_DATA;
	recipe->command = command = fuzz_voucher_command(data[0]);
	recipe->previous_voucher = MACH_VOUCHER_NAME_NULL;
	recipe->content_size = (mach_voucher_attr_content_size_t)content_size;
	memcpy(recipe->content, data, content_size);

	harness_copyio_begin(&copyio, VOUCHER_USER_BASE, user, sizeof(user));
	ipc_space_set_policy(current_space(),
	    voucher_policies[data[0] % (sizeof(voucher_policies) / sizeof(voucher_policies[0]))]);

	mach_port_name_t host_name = fuzz_host_name();
	{
		struct host_create_mach_voucher_args create = {
			.host = host_name,
			.recipes = (mach_voucher_attr_raw_recipe_array_t)(uintptr_t)
			    (VOUCHER_USER_BASE + VOUCHER_RECIPE_OFFSET),
			.recipes_size = (int)(sizeof(*recipe) + content_size),
			.voucher = VOUCHER_USER_BASE,
		};
		kern_return_t kr = host_create_mach_voucher_trap(&create);

		if (command == MACH_VOUCHER_ATTR_USER_DATA_STORE && kr != KERN_SUCCESS) {
			raw_printf("FUZZ BUG: valid voucher store failed\n");
			abort();
		}
		if (kr == KERN_SUCCESS) {
			mach_port_name_t voucher_name;
			memcpy(&voucher_name, user, sizeof(voucher_name));
			if (!MACH_PORT_VALID(voucher_name)) {
				raw_printf("FUZZ BUG: voucher creation returned an invalid name\n");
				abort();
			}
			if (command == MACH_VOUCHER_ATTR_USER_DATA_STORE) {
				mach_msg_type_number_t capacity = sizeof(user) - VOUCHER_RECIPE_OFFSET;
				struct mach_voucher_extract_attr_recipe_args extract = {
					.voucher_name = voucher_name,
					.key = MACH_VOUCHER_ATTR_KEY_USER_DATA,
					.recipe = (mach_voucher_attr_raw_recipe_t)(uintptr_t)
					    (VOUCHER_USER_BASE + VOUCHER_RECIPE_OFFSET),
					.recipe_size = VOUCHER_USER_BASE + sizeof(uint64_t),
				};
				memcpy(&user[sizeof(uint64_t)], &capacity, sizeof(capacity));
				kr = mach_voucher_extract_attr_recipe_trap(&extract);
				if (kr != KERN_SUCCESS || recipe->content_size != content_size ||
				    memcmp(recipe->content, data, content_size) != 0) {
					raw_printf("FUZZ BUG: voucher content did not round trip\n");
					abort();
				}
			}
			if (mach_port_deallocate(current_space(), voucher_name) != KERN_SUCCESS) {
				raw_printf("FUZZ BUG: voucher teardown failed\n");
				abort();
			}
		}
	}

	ipc_space_set_policy(current_space(), old_policy);
	harness_copyio_end(&copyio);
	return 0;
}
