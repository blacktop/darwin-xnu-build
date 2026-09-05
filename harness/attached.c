/*
 * Symbols the kernel link normally takes from the KDK's lib<SoC>.os.<config>.a, which the
 * xnu_libraries archive does not contain. osfmk/conf/files.arm64 marks the pmap and low-level
 * arm64 sources `optional nos_arm_asm`, and that option is off precisely because the KDK archive
 * supplies them, so they are compiled into neither the kernel nor the library.
 *
 * Defined in assembly so that no declaration in xnu's headers can conflict with the definition.
 * Everything here is storage or a trap, never behavior: the mocks in xnu/tests/unit/mocks
 * interpose the real implementations, and any stub reached at runtime is a bug in the harness,
 * not something to paper over, so the function stubs trap instead of returning.
 *
 * The symbol list is derived from the KDK archive: data vs text is taken from `nm -m` on
 * libT6020.os.DEVELOPMENT.a, not guessed.
 *
 * Linked into the kernel dylib alongside Apple's mocks/mock_attached.c.
 */

/*
 * copyio recovery table: defined in osfmk/arm64/machine_routines_asm.s (a KDK-archive object) and
 * walked by sleh.c on copyin/copyout faults, which references it page-relatively so it must have a
 * link-time address. Userspace never takes that path and copyin/copyout are mocked, so an empty
 * table is correct.
 */
__asm__(
	".section __DATA_CONST,__const\n"
	".p2align 3\n"
	".globl _copyio_recover_table\n"
	".globl _copyio_recover_table_end\n"
	"_copyio_recover_table:\n"
	"_copyio_recover_table_end:\n"
	".quad 0\n"
	".text\n");

/*
 * Data the library imports from the KDK archive. `kernel_pmap` and `native_pt_attr` are
 * dereferenced by mocks/fake_kinit.c, so they point at real storage; the rest are counters,
 * flags and pointers that the bootstrap or the mocks initialize.
 */
__asm__(
	".data\n"
	".p2align 3\n"
	".globl _kernel_pmap\n"
	"_kernel_pmap: .quad _harness_pmap_store\n"
	".globl _native_pt_attr\n"
	"_native_pt_attr: .quad _harness_pt_attr_store\n"
	".globl _ads_zone\n"
	"_ads_zone: .space 8\n"
	".globl _allow_data_exec\n"
	"_allow_data_exec: .space 8\n"
	".globl _allow_stack_exec\n"
	"_allow_stack_exec: .space 8\n"
	".globl _asid_bitmap\n"
	"_asid_bitmap: .space 8\n"
	".globl _cpu_tte\n"
	"_cpu_tte: .space 8\n"
	".globl _cpu_ttep\n"
	"_cpu_ttep: .space 8\n"
	".globl _current_delayed_free_pt_count\n"
	"_current_delayed_free_pt_count: .space 8\n"
	".globl _disarm_protected_io\n"
	"_disarm_protected_io: .space 8\n"
	".globl _disarm_protected_io_ever\n"
	"_disarm_protected_io_ever: .space 8\n"
	".globl _inuse_kernel_ptepages_count\n"
	"_inuse_kernel_ptepages_count: .space 8\n"
	".globl _inuse_kernel_ttepages_count\n"
	"_inuse_kernel_ttepages_count: .space 8\n"
	".globl _inuse_kernel_tteroot_count\n"
	"_inuse_kernel_tteroot_count: .space 8\n"
	".globl _inuse_user_ptepages_count\n"
	"_inuse_user_ptepages_count: .space 8\n"
	".globl _inuse_user_ttepages_count\n"
	"_inuse_user_ttepages_count: .space 8\n"
	".globl _inuse_user_tteroot_count\n"
	"_inuse_user_tteroot_count: .space 8\n"
	".globl _io_attr_table\n"
	"_io_attr_table: .space 8\n"
	".globl _num_io_rgns\n"
	"_num_io_rgns: .space 8\n"
	".globl _nx_enabled\n"
	"_nx_enabled: .space 8\n"
	".globl _percpu_slot_pmap_sptm_percpu\n"
	"_percpu_slot_pmap_sptm_percpu: .space 8\n"
	".globl _pmap_asid_flushes\n"
	"_pmap_asid_flushes: .space 8\n"
	".globl _pmap_asid_hits\n"
	"_pmap_asid_hits: .space 8\n"
	".globl _pmap_asid_misses\n"
	"_pmap_asid_misses: .space 8\n"
	".globl _pmap_object_store\n"
	"_pmap_object_store: .space 8\n"
	".globl _pmap_query_page_info_retries\n"
	"_pmap_query_page_info_retries: .space 8\n"
	".globl _pmap_speculation_restrictions\n"
	"_pmap_speculation_restrictions: .space 8\n"
	".globl _pmap_zone\n"
	"_pmap_zone: .space 8\n"
	".globl _rorgn_begin\n"
	"_rorgn_begin: .space 8\n"
	".globl _rorgn_end\n"
	"_rorgn_end: .space 8\n"
	".globl _total_delayed_free_pt_count\n"
	"_total_delayed_free_pt_count: .space 8\n"
	".globl _use_xnu_restricted\n"
	"_use_xnu_restricted: .space 8\n"
	".globl _vm_first_phys\n"
	"_vm_first_phys: .space 8\n"
	".globl _vm_footprint_suspend_allowed\n"
	"_vm_footprint_suspend_allowed: .space 8\n"
	".globl _vm_last_phys\n"
	"_vm_last_phys: .space 8\n"
	".globl _pmap_pt_attr_4k\n"
	"_pmap_pt_attr_4k: .space 512\n"
	"_harness_pmap_store: .space 4096\n"
	"_harness_pt_attr_store: .space 512\n"
	".text\n");

/*
 * pmap internals that only osfmk/arm64/sptm/pmap/pmap.c defines. Apple's mock_pmap.c and
 * mock_vm.c interpose them, but the interposition machinery still needs the symbol to exist.
 * Trap if one is ever actually entered.
 */
__asm__(
	".text\n"
	".p2align 2\n"
	".globl _pmap_enter_options_internal\n"
	"_pmap_enter_options_internal: brk #0\n"
	".globl _pmap_flush_sptm_traces\n"
	"_pmap_flush_sptm_traces: brk #0\n"
	".globl _pmap_is_page_free\n"
	"_pmap_is_page_free: brk #0\n"
	".globl _pmap_map_cpu_windows_copy_internal\n"
	"_pmap_map_cpu_windows_copy_internal: brk #0\n"
	".globl _pmap_protect_options_internal\n"
	"_pmap_protect_options_internal: brk #0\n"
	".globl _pmap_query_resident_internal\n"
	"_pmap_query_resident_internal: brk #0\n");

/*
 * Apple's thread mocks interpose the PAC-safe interrupt helpers even in a plain-arm64 build and
 * retain pointers to the originals. The real helpers only exist when pointer authentication is
 * enabled, so provide link-only traps for that configuration. The mock implementations model the
 * logical interrupt state; reaching either original would be a harness bug.
 */
#if !__has_feature(ptrauth_calls)
__asm__(
	".text\n"
	".p2align 2\n"
	".globl _ml_pac_safe_interrupts_disable\n"
	"_ml_pac_safe_interrupts_disable: brk #0\n"
	".globl _ml_pac_safe_interrupts_restore\n"
	"_ml_pac_safe_interrupts_restore: brk #0\n");
#endif /* !__has_feature(ptrauth_calls) */

/*
 * Low-address backing for Apple's mock memory pools.
 *
 * mocks/mock_mem.c allocates each pool with calloc(). XNU packs vm_map, vm_map_entry and
 * vm_object pointers into 31 bits with a 6-bit shift relative to a 16 KiB base, so a pointer must
 * stay below ~0x2000003fc0 to be encodable, and mocks/osfmk/mock_alloc.c asserts exactly that. On
 * macOS a multi-megabyte calloc is served by mmap far above that limit (~500 GB observed), so the
 * pools are cut from one fixed reservation inside the encodable range instead.
 * harness/build.sh redirects mock_mem.c's pool allocation here.
 *
 * libc is declared by hand: the kernel flags build with -nostdlibinc, so its headers are absent.
 */
#define HARNESS_PACKING_LIMIT 0x2000000000ULL /* vm_packing_max_packable for these params */
#define HARNESS_ARENA_SIZE 0x40000000ULL       /* 1 GiB */

/*
 * mmap must come from libc explicitly: the kernel library defines its own mmap (the BSD syscall in
 * bsd/kern/kern_mman.c), and a plain call from inside this dylib binds to that one and crashes.
 * RTLD_NEXT resolves past this image into libSystem. write/abort have no kernel-side collision.
 */
#define HARNESS_RTLD_NEXT ((void *)-1L)
typedef void *(*harness_mmap_fn)(void *, unsigned long, int, int, int, long long);

extern void *dlsym(void *handle, const char *symbol);
extern long write(int fd, const void *buf, unsigned long len);
extern void abort(void) __attribute__((noreturn));

void *harness_low_alloc(unsigned long size);

void *
harness_low_alloc(unsigned long size)
{
	static unsigned long long cursor, arena_base, arena_end;
	const int prot = 0x1 | 0x2;                    /* PROT_READ | PROT_WRITE */
	const int flags = 0x0002 | 0x1000;             /* MAP_PRIVATE | MAP_ANON */

	if (cursor == 0) {
		harness_mmap_fn libc_mmap = (harness_mmap_fn)dlsym(HARNESS_RTLD_NEXT, "mmap");
		if (libc_mmap == (harness_mmap_fn)0) {
			static const char msg[] = "harness: libc mmap not found\n";
			write(2, msg, sizeof(msg) - 1);
			abort();
		}

		/*
		 * Try fixed low bases first, then let the kernel choose and accept anything that still
		 * lands inside the encodable range.
		 */
		void *p = (void *)-1L;
		unsigned long long base = 0;
		for (unsigned long long cand = 0x400000000ULL; cand < HARNESS_PACKING_LIMIT; cand <<= 1) {
			p = libc_mmap((void *)cand, HARNESS_ARENA_SIZE, prot, flags | 0x0010 /* MAP_FIXED */, -1, 0);
			if (p == (void *)cand) {
				base = cand;
				break;
			}
		}
		if (base == 0) {
			p = libc_mmap((void *)0, HARNESS_ARENA_SIZE, prot, flags, -1, 0);
			if (p != (void *)-1L &&
			    (unsigned long long)p + HARNESS_ARENA_SIZE < HARNESS_PACKING_LIMIT) {
				base = (unsigned long long)p;
			}
		}
		if (base == 0) {
			static const char msg[] = "harness: could not reserve an arena below the "
			    "pointer-packing limit\n";
			write(2, msg, sizeof(msg) - 1);
			abort();
		}
		arena_base = base;
		arena_end = base + HARNESS_ARENA_SIZE;
		cursor = base;
	}

	if (size > HARNESS_ARENA_SIZE - 16383) {
		static const char msg[] = "harness: low allocation too large\n";
		write(2, msg, sizeof(msg) - 1);
		abort();
	}
	size = (size + 16383) & ~16383ULL;
	if (cursor > arena_end || size > arena_end - cursor) {
		static const char msg[] = "harness: low arena exhausted\n";
		write(2, msg, sizeof(msg) - 1);
		abort();
	}

	void *ret = (void *)cursor;
	cursor += size;
	return ret;
}
