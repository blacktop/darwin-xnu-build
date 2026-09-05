/* Fuzz target: reply-port policy with a buffer-backed service-port description. */
#define UT_MODULE osfmk
#include "harness_copyio.h"
#include "mocks/osfmk/unit_test_utils.h"
#include <ipc/ipc_policy.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/task.h>
#include <mach/mach_port.h>

#define POLICY_USER_BASE 0x10000000ULL
#define XPC_DOMAIN_SYSTEM 1
#define XPC_DOMAIN_PORT 7

extern mach_msg_return_t ipc_validate_local_port(mach_port_t, mach_port_t,
    mach_msg_option64_t);

static const ipc_space_policy_t ipc_policies[] = {
	IPC_SPACE_POLICY_DEFAULT,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V0,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V1,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V2,
	IPC_SPACE_POLICY_DEFAULT | IPC_POLICY_ENHANCED_V3,
	IPC_SPACE_POLICY_DEFAULT | IPC_SPACE_POLICY_PLATFORM | IPC_POLICY_ENHANCED_V3,
	IPC_SPACE_POLICY_DEFAULT | IPC_SPACE_POLICY_CONTAINED,
};

static kern_return_t
fuzz_policy_port(mpo_flags_t flags, user_addr_t info, mach_port_name_t *name,
    ipc_port_t *port)
{
	mach_port_options_t options = {
		.flags = flags,
		.service_port_info64 = info,
	};
	kern_return_t kr = mach_port_construct(current_space(), &options, 0, name);

	if (kr == KERN_SUCCESS) {
		kr = ipc_port_translate_receive(current_space(), *name, port);
		if (kr == KERN_SUCCESS) {
			ip_mq_unlock(*port);
		}
	}
	return kr;
}

static void
fuzz_policy_port_destroy(mach_port_name_t name, mach_port_delta_t send_delta)
{
	if (MACH_PORT_VALID(name) &&
	    mach_port_destruct(current_space(), name, send_delta, 0) != KERN_SUCCESS) {
		raw_printf("FUZZ BUG: policy port teardown failed\n");
		abort();
	}
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct mach_service_port_info info = {};
	struct harness_copyio copyio;
	mach_port_name_t reply_name = MACH_PORT_NULL;
	mach_port_name_t dest_name = MACH_PORT_NULL;
	ipc_port_t reply_port = IPC_PORT_NULL;
	ipc_port_t dest_port = IPC_PORT_NULL;
	ipc_space_policy_t old_policy;
	user_addr_t info_address;

	if (size < 3) {
		return 0;
	}
	size_t name_size = size - 3 < sizeof(info.mspi_string_name) - 1 ?
	    size - 3 : sizeof(info.mspi_string_name) - 1;
	memcpy(info.mspi_string_name, &data[3], name_size);
	info.mspi_domain_type = (data[2] & 1) ? XPC_DOMAIN_PORT : XPC_DOMAIN_SYSTEM;

	harness_copyio_begin(&copyio, POLICY_USER_BASE, &info, sizeof(info));
	info_address = (data[2] & 0x80) ? POLICY_USER_BASE + sizeof(info) : POLICY_USER_BASE;
	old_policy = ipc_space_policy(current_space());
	ipc_space_set_policy(current_space(), IPC_SPACE_POLICY_DEFAULT);

	mpo_flags_t dest_flags = MPO_STRICT_SERVICE_PORT | MPO_INSERT_SEND_RIGHT;
	kern_return_t dest_kr = fuzz_policy_port(dest_flags, info_address, &dest_name, &dest_port);
	if (info_address != POLICY_USER_BASE) {
		if (dest_kr != KERN_MEMORY_ERROR) {
			raw_printf("FUZZ BUG: invalid copyin range returned %d\n", dest_kr);
			abort();
		}
	} else if (dest_kr != KERN_SUCCESS) {
		raw_printf("FUZZ BUG: valid copyin range returned %d\n", dest_kr);
		abort();
	}

	if (dest_kr == KERN_SUCCESS) {
		mpo_flags_t reply_flags = (data[1] & 1) ? MPO_REPLY_PORT :
		    MPO_PORT | MPO_INSERT_SEND_RIGHT;
		kern_return_t reply_kr = fuzz_policy_port(reply_flags, 0, &reply_name, &reply_port);
		if (reply_kr != KERN_SUCCESS) {
			raw_printf("FUZZ BUG: canonical reply port construction failed\n");
			abort();
		}
		ipc_space_policy_t policy =
		    ipc_policies[data[0] % (sizeof(ipc_policies) / sizeof(ipc_policies[0]))];
		mach_msg_return_t mr;

		ipc_space_set_policy(current_space(), policy);
		mr = ipc_validate_local_port(reply_port, dest_port,
		    (mach_msg_option64_t)policy << MACH64_POLICY_SHIFT);
		if (mr != MACH_MSG_SUCCESS && mr != MACH_SEND_INVALID_REPLY) {
			raw_printf("FUZZ BUG: policy validation returned %d\n", mr);
			abort();
		}
	}

	ipc_space_set_policy(current_space(), IPC_SPACE_POLICY_DEFAULT);
	fuzz_policy_port_destroy(reply_name, (data[1] & 1) ? 0 : -1);
	fuzz_policy_port_destroy(dest_name, -1);
	ipc_space_set_policy(current_space(), old_policy);
	harness_copyio_end(&copyio);
	return 0;
}
