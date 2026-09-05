/* A small public-toolchain replacement for darwintest's libFuzzer runner. */
#define UT_MODULE osfmk
#include "mocks/std_safe.h"

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
extern int LLVMFuzzerRunDriver(int *argc, char ***argv,
    int (*callback)(const uint8_t *data, size_t size));

int
main(int argc, char **argv)
{
	return LLVMFuzzerRunDriver(&argc, &argv, LLVMFuzzerTestOneInput);
}
