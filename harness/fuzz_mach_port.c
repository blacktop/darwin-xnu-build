/*
 * Fuzz target: the Mach port right lifecycle.
 *
 * Drives mach_port_construct / mach_port_mod_refs / mach_port_request_notification /
 * mach_port_destruct against current_space(), the same in-process IPC surface that
 * xnu/tests/unit/ipc/mach_port_construct.c exercises. The input is read as a program: each step is
 * an opcode plus operands, so a mutation reorders or reshapes a sequence of port operations rather
 * than just flipping one argument. Sequence bugs (refcount imbalance, use-after-destruct, guard
 * mismatches) are the interesting class here, and they only appear across operations.
 *
 * The entry point is LLVMFuzzerTestOneInput so a libFuzzer build needs no changes to this file.
 * There is no arm64e libFuzzer runtime in the public toolchain, so harness/fuzz_runner.c supplies a
 * main() that replays files instead; see harness/README.md.
 *
 * Oracles: XNU's own panics and asserts (the mocks turn panic into an abort), plus the invariant
 * checks below. A kern_return_t error is never a finding on its own - rejecting bad input is the
 * kernel doing its job.
 */
#define UT_MODULE osfmk
#include "mocks/std_safe.h"
#include "mocks/osfmk/unit_test_utils.h"
#include <mach/mach_port.h>
#include <mach/port.h>
#include <kern/task.h>
#include <ipc/ipc_space.h>

#define FUZZ_MAX_PORTS 8

/* Byte reader. Running out of input ends the program rather than wrapping or faulting. */
struct fuzz_input {
	const uint8_t *fi_data;
	size_t fi_size;
	size_t fi_pos;
};

static bool
fuzz_bytes(struct fuzz_input *in, void *out, size_t len)
{
	if (in->fi_pos + len > in->fi_size) {
		return false;
	}
	memcpy(out, in->fi_data + in->fi_pos, len);
	in->fi_pos += len;
	return true;
}

static bool
fuzz_u8(struct fuzz_input *in, uint8_t *out)
{
	return fuzz_bytes(in, out, sizeof(*out));
}

static bool
fuzz_u32(struct fuzz_input *in, uint32_t *out)
{
	return fuzz_bytes(in, out, sizeof(*out));
}

static bool
fuzz_u64(struct fuzz_input *in, uint64_t *out)
{
	return fuzz_bytes(in, out, sizeof(*out));
}

/* Ports this program has created, so later steps can name one it actually owns. */
struct fuzz_ports {
	mach_port_name_t fp_name[FUZZ_MAX_PORTS];
	uint64_t fp_guard[FUZZ_MAX_PORTS];
	bool fp_guarded[FUZZ_MAX_PORTS];
	unsigned fp_count;
};

/*
 * Pick a port name. Most of the time use one this program created, but let the input reach an
 * arbitrary name too: name validation is part of what is being tested.
 */
static mach_port_name_t
fuzz_pick_name(struct fuzz_input *in, struct fuzz_ports *ports, unsigned *slot_out)
{
	uint8_t sel = 0;
	uint32_t raw = 0;

	*slot_out = FUZZ_MAX_PORTS;
	if (!fuzz_u8(in, &sel)) {
		return MACH_PORT_NULL;
	}
	if ((sel & 0x3) == 0 || ports->fp_count == 0) {
		if (!fuzz_u32(in, &raw)) {
			return MACH_PORT_NULL;
		}
		return (mach_port_name_t)raw;
	}
	*slot_out = (sel >> 2) % ports->fp_count;
	return ports->fp_name[*slot_out];
}

static void
fuzz_forget(struct fuzz_ports *ports, unsigned slot)
{
	if (slot >= ports->fp_count) {
		return;
	}
	ports->fp_name[slot] = ports->fp_name[ports->fp_count - 1];
	ports->fp_guard[slot] = ports->fp_guard[ports->fp_count - 1];
	ports->fp_guarded[slot] = ports->fp_guarded[ports->fp_count - 1];
	ports->fp_count--;
}

static void
fuzz_construct(struct fuzz_input *in, struct fuzz_ports *ports)
{
	mach_port_options_t options = {};
	mach_port_name_t name = MACH_PORT_NULL;
	uint32_t flags = 0;
	uint32_t qlimit = 0;
	uint64_t context = 0;

	if (!fuzz_u32(in, &flags) || !fuzz_u32(in, &qlimit) || !fuzz_u64(in, &context)) {
		return;
	}
	options.flags = flags;
	options.mpl.mpl_qlimit = (mach_port_msgcount_t)qlimit;

	if (mach_port_construct(current_space(), &options, context, &name) != KERN_SUCCESS) {
		return;
	}

	/*
	 * A successful construct must hand back a usable name. Anything else means the kernel
	 * reported success while leaving the space inconsistent.
	 */
	if (!MACH_PORT_VALID(name)) {
		raw_printf("FUZZ BUG: mach_port_construct returned success with invalid name 0x%x "
		    "(flags 0x%x)\n", name, flags);
		abort();
	}

	if (ports->fp_count < FUZZ_MAX_PORTS) {
		unsigned slot = ports->fp_count++;
		ports->fp_name[slot] = name;
		ports->fp_guard[slot] = context;
		ports->fp_guarded[slot] = (flags & MPO_CONTEXT_AS_GUARD) != 0;
	} else {
		mach_port_destruct(current_space(), name, 0,
		    (flags & MPO_CONTEXT_AS_GUARD) ? context : 0);
	}
}

static void
fuzz_mod_refs(struct fuzz_input *in, struct fuzz_ports *ports)
{
	unsigned slot;
	mach_port_name_t name = fuzz_pick_name(in, ports, &slot);
	uint8_t right = 0;
	uint8_t delta = 0;

	if (!fuzz_u8(in, &right) || !fuzz_u8(in, &delta)) {
		return;
	}
	/* A -1 delta on the receive right destroys the port, so drop our record of it. */
	if (mach_port_mod_refs(current_space(), name, right % (MACH_PORT_RIGHT_NUMBER + 1),
	    (mach_port_delta_t)(int8_t)delta) == KERN_SUCCESS &&
	    (int8_t)delta < 0 && slot < FUZZ_MAX_PORTS) {
		fuzz_forget(ports, slot);
	}
}

static void
fuzz_request_notification(struct fuzz_input *in, struct fuzz_ports *ports)
{
	unsigned slot;
	mach_port_name_t name = fuzz_pick_name(in, ports, &slot);
	mach_port_t previous = MACH_PORT_NULL;
	uint32_t id = 0;
	uint32_t sync = 0;

	if (!fuzz_u32(in, &id) || !fuzz_u32(in, &sync)) {
		return;
	}
	/*
	 * The declaration in mach/mach_port.h carries the MIG poly argument, so it takes seven
	 * parameters even though the implementation in ipc/mach_port.c takes six.
	 */
	mach_port_request_notification(current_space(), name, (mach_msg_id_t)id,
	    (mach_port_mscount_t)sync, MACH_PORT_NULL, MACH_MSG_TYPE_MOVE_SEND, &previous);
}

static void
fuzz_destruct(struct fuzz_input *in, struct fuzz_ports *ports)
{
	unsigned slot;
	mach_port_name_t name = fuzz_pick_name(in, ports, &slot);
	uint8_t srdelta = 0;
	uint64_t guard = 0;

	if (!fuzz_u8(in, &srdelta)) {
		return;
	}
	if (slot < FUZZ_MAX_PORTS && ports->fp_guarded[slot]) {
		guard = ports->fp_guard[slot];
	} else if (!fuzz_u64(in, &guard)) {
		return;
	}
	if (mach_port_destruct(current_space(), name, (mach_port_delta_t)(int8_t)srdelta,
	    guard) == KERN_SUCCESS && slot < FUZZ_MAX_PORTS) {
		fuzz_forget(ports, slot);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct fuzz_input in = { .fi_data = data, .fi_size = size };
	struct fuzz_ports ports = {};
	uint8_t op;

	while (fuzz_u8(&in, &op)) {
		switch (op % 4) {
		case 0:
			fuzz_construct(&in, &ports);
			break;
		case 1:
			fuzz_mod_refs(&in, &ports);
			break;
		case 2:
			fuzz_request_notification(&in, &ports);
			break;
		default:
			fuzz_destruct(&in, &ports);
			break;
		}
	}

	/* Leave the space as it was found, so iterations stay independent. */
	while (ports.fp_count > 0) {
		unsigned slot = ports.fp_count - 1;
		mach_port_destruct(current_space(), ports.fp_name[slot], 0,
		    ports.fp_guarded[slot] ? ports.fp_guard[slot] : 0);
		ports.fp_count--;
	}
	return 0;
}
