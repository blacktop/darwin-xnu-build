/*
 * Replay driver for fuzz targets.
 *
 * This driver keeps deterministic file replay available in both baseline and instrumented builds.
 * harness/build.sh also links a separate coverage-guided driver when GUIDED_FUZZING=1.
 *
 * With no arguments it runs a few built-in inputs as a smoke test, so the target is exercised on
 * every build rather than only when someone has a corpus.
 */
#define UT_MODULE osfmk
#include "mocks/std_safe.h"
#include "mocks/osfmk/unit_test_utils.h"

#define FUZZ_MAX_INPUT (1u << 20)

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* libc, declared by hand: the kernel flags build with -nostdlibinc. */
extern int open(const char *path, int flags, ...);
extern long read(int fd, void *buf, unsigned long count);
extern int close(int fd);

static bool
fuzz_run_file(const char *path, uint8_t *buf)
{
	int fd = open(path, 0 /* O_RDONLY */);
	if (fd < 0) {
		raw_printf("fuzz: cannot open %s\n", path);
		return false;
	}

	long total = 0;
	for (;;) {
		long n = read(fd, buf + total, FUZZ_MAX_INPUT - (unsigned long)total);
		if (n <= 0) {
			break;
		}
		total += n;
		if ((unsigned long)total >= FUZZ_MAX_INPUT) {
			break;
		}
	}
	close(fd);

	raw_printf("fuzz: %s (%ld bytes)\n", path, total);
	LLVMFuzzerTestOneInput(buf, (size_t)total);
	return true;
}

int
main(int argc, char **argv)
{
	static uint8_t buf[FUZZ_MAX_INPUT];

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (!fuzz_run_file(argv[i], buf)) {
				return 1;
			}
		}
		raw_printf("fuzz: replayed %d input(s)\n", argc - 1);
		return 0;
	}

	/*
	 * Built-in inputs: an empty program, one that only constructs, and a mixed sequence. These
	 * are not a corpus, just enough to prove the target runs and cleans up after itself.
	 */
	static const uint8_t seeds[][24] = {
		{ 0 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
		  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },
		{ 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
		  0xde, 0xad, 0xbe, 0xef, 0x00, 0x00, 0x00, 0x00,
		  0x01, 0x04, 0x01, 0x03, 0x04, 0xff },
	};

	for (unsigned i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
		raw_printf("fuzz: seed %u\n", i);
		LLVMFuzzerTestOneInput(seeds[i], sizeof(seeds[i]));
	}
	raw_printf("PASS: fuzz target ran %zu built-in input(s)\n", sizeof(seeds) / sizeof(seeds[0]));
	return 0;
}
