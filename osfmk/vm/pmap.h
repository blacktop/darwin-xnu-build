/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	vm/pmap.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 */

#ifndef _VM_PMAP_H_
#define _VM_PMAP_H_

#include <mach/kern_return.h>
#include <mach/vm_param.h>
#include <mach/vm_types.h>
#include <mach/vm_attributes.h>
#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <kern/trustcache.h>

#if __has_include(<CoreEntitlements/CoreEntitlements.h>)
#include <CoreEntitlements/CoreEntitlements.h>
#endif

#ifdef  KERNEL_PRIVATE

/*
 *	The following is a description of the interface to the
 *	machine-dependent "physical map" data structure.  The module
 *	must provide a "pmap_t" data type that represents the
 *	set of valid virtual-to-physical addresses for one user
 *	address space.  [The kernel address space is represented
 *	by a distinguished "pmap_t".]  The routines described manage
 *	this type, install and update virtual-to-physical mappings,
 *	and perform operations on physical addresses common to
 *	many address spaces.
 */

/* Copy between a physical page and a virtual address */
/* LP64todo - switch to vm_map_offset_t when it grows */
extern kern_return_t    copypv(
	addr64_t source,
	addr64_t sink,
	unsigned int size,
	int which);
#define cppvPsnk        1
#define cppvPsnkb      31
#define cppvPsrc        2
#define cppvPsrcb      30
#define cppvFsnk        4
#define cppvFsnkb      29
#define cppvFsrc        8
#define cppvFsrcb      28
#define cppvNoModSnk   16
#define cppvNoModSnkb  27
#define cppvNoRefSrc   32
#define cppvNoRefSrcb  26
#define cppvKmap       64       /* Use the kernel's vm_map */
#define cppvKmapb      25

extern boolean_t pmap_has_managed_page(ppnum_t first, ppnum_t last);

#if MACH_KERNEL_PRIVATE || BSD_KERNEL_PRIVATE
#include <mach/mach_types.h>
#include <vm/memory_types.h>

/*
 * Routines used during BSD process creation.
 */

extern pmap_t           pmap_create_options(    /* Create a pmap_t. */
	ledger_t        ledger,
	vm_map_size_t   size,
	unsigned int    flags);

#if __has_feature(ptrauth_calls) && defined(XNU_TARGET_OS_OSX)
/**
 * Informs the pmap layer that a process will be running with user JOP disabled,
 * as if PMAP_CREATE_DISABLE_JOP had been passed during pmap creation.
 *
 * @note This function cannot be used once the target process has started
 * executing code.  It is intended for cases where user JOP is disabled based on
 * the code signature (e.g., special "keys-off" entitlements), which is too late
 * to change the flags passed to pmap_create_options.
 *
 * @param pmap	The pmap belonging to the target process
 */
extern void             pmap_disable_user_jop(
	pmap_t          pmap);
#endif /* __has_feature(ptrauth_calls) && defined(XNU_TARGET_OS_OSX) */
#endif /* MACH_KERNEL_PRIVATE || BSD_KERNEL_PRIVATE */

#ifdef  MACH_KERNEL_PRIVATE

#include <mach_assert.h>

#include <machine/pmap.h>
/*
 *	Routines used for initialization.
 *	There is traditionally also a pmap_bootstrap,
 *	used very early by machine-dependent code,
 *	but it is not part of the interface.
 *
 *	LP64todo -
 *	These interfaces are tied to the size of the
 *	kernel pmap - and therefore use the "local"
 *	vm_offset_t, etc... types.
 */

extern void *pmap_steal_memory(vm_size_t size); /* Early memory allocation */
extern void *pmap_steal_freeable_memory(vm_size_t size); /* Early memory allocation */

extern uint_t pmap_free_pages(void); /* report remaining unused physical pages */
#if defined(__arm__) || defined(__arm64__)
extern uint_t pmap_free_pages_span(void); /* report phys address range of unused physical pages */
#endif /* defined(__arm__) || defined(__arm64__) */

extern void pmap_startup(vm_offset_t *startp, vm_offset_t *endp); /* allocate vm_page structs */

extern void pmap_init(void); /* Initialization, once we have kernel virtual memory.  */

extern void mapping_adjust(void); /* Adjust free mapping count */

extern void mapping_free_prime(void); /* Primes the mapping block release list */

#ifndef MACHINE_PAGES
/*
 *	If machine/pmap.h defines MACHINE_PAGES, it must implement
 *	the above functions.  The pmap module has complete control.
 *	Otherwise, it must implement the following functions:
 *		pmap_free_pages
 *		pmap_virtual_space
 *		pmap_next_page
 *		pmap_init
 *	and vm/vm_resident.c implements pmap_steal_memory and pmap_startup
 *	using pmap_free_pages, pmap_next_page, pmap_virtual_space,
 *	and pmap_enter.  pmap_free_pages may over-estimate the number
 *	of unused physical pages, and pmap_next_page may return FALSE
 *	to indicate that there are no more unused pages to return.
 *	However, for best performance pmap_free_pages should be accurate.
 */

/*
 * Routines to return the next unused physical page.
 */
extern boolean_t pmap_next_page(ppnum_t *pnum);
extern boolean_t pmap_next_page_hi(ppnum_t *pnum, boolean_t might_free);
#ifdef __x86_64__
extern kern_return_t pmap_next_page_large(ppnum_t *pnum);
extern void pmap_hi_pages_done(void);
#endif

/*
 * Report virtual space available for the kernel.
 */
extern void pmap_virtual_space(
	vm_offset_t     *virtual_start,
	vm_offset_t     *virtual_end);
#endif  /* MACHINE_PAGES */

/*
 * Routines to manage the physical map data structure.
 */
extern pmap_t(pmap_kernel)(void);               /* Return the kernel's pmap */
extern void             pmap_reference(pmap_t pmap);    /* Gain a reference. */
extern void             pmap_destroy(pmap_t pmap); /* Release a reference. */
extern void             pmap_switch(pmap_t);
extern void             pmap_require(pmap_t pmap);

#if MACH_ASSERT
extern void pmap_set_process(pmap_t pmap,
    int pid,
    char *procname);
#endif /* MACH_ASSERT */

extern kern_return_t    pmap_enter(     /* Enter a mapping */
	pmap_t          pmap,
	vm_map_offset_t v,
	ppnum_t         pn,
	vm_prot_t       prot,
	vm_prot_t       fault_type,
	unsigned int    flags,
	boolean_t       wired);

extern kern_return_t    pmap_enter_options(
	pmap_t pmap,
	vm_map_offset_t v,
	ppnum_t pn,
	vm_prot_t prot,
	vm_prot_t fault_type,
	unsigned int flags,
	boolean_t wired,
	unsigned int options,
	void *arg);
extern kern_return_t    pmap_enter_options_addr(
	pmap_t pmap,
	vm_map_offset_t v,
	pmap_paddr_t pa,
	vm_prot_t prot,
	vm_prot_t fault_type,
	unsigned int flags,
	boolean_t wired,
	unsigned int options,
	void *arg);

extern void             pmap_remove_some_phys(
	pmap_t          pmap,
	ppnum_t         pn);

extern void             pmap_lock_phys_page(
	ppnum_t         pn);

extern void             pmap_unlock_phys_page(
	ppnum_t         pn);


/*
 *	Routines that operate on physical addresses.
 */

extern void             pmap_page_protect(      /* Restrict access to page. */
	ppnum_t phys,
	vm_prot_t       prot);

extern void             pmap_page_protect_options(      /* Restrict access to page. */
	ppnum_t phys,
	vm_prot_t       prot,
	unsigned int    options,
	void            *arg);

extern void(pmap_zero_page)(
	ppnum_t         pn);

extern void(pmap_zero_part_page)(
	ppnum_t         pn,
	vm_offset_t     offset,
	vm_size_t       len);

extern void(pmap_copy_page)(
	ppnum_t         src,
	ppnum_t         dest);

extern void(pmap_copy_part_page)(
	ppnum_t         src,
	vm_offset_t     src_offset,
	ppnum_t         dst,
	vm_offset_t     dst_offset,
	vm_size_t       len);

extern void(pmap_copy_part_lpage)(
	vm_offset_t     src,
	ppnum_t         dst,
	vm_offset_t     dst_offset,
	vm_size_t       len);

extern void(pmap_copy_part_rpage)(
	ppnum_t         src,
	vm_offset_t     src_offset,
	vm_offset_t     dst,
	vm_size_t       len);

extern unsigned int(pmap_disconnect)(   /* disconnect mappings and return reference and change */
	ppnum_t         phys);

extern unsigned int(pmap_disconnect_options)(   /* disconnect mappings and return reference and change */
	ppnum_t         phys,
	unsigned int    options,
	void            *arg);

extern kern_return_t(pmap_attribute_cache_sync)(      /* Flush appropriate
                                                       * cache based on
                                                       * page number sent */
	ppnum_t         pn,
	vm_size_t       size,
	vm_machine_attribute_t attribute,
	vm_machine_attribute_val_t* value);

extern unsigned int(pmap_cache_attributes)(
	ppnum_t         pn);

/*
 * Set (override) cache attributes for the specified physical page
 */
extern  void            pmap_set_cache_attributes(
	ppnum_t,
	unsigned int);

extern void            *pmap_map_compressor_page(
	ppnum_t);

extern void             pmap_unmap_compressor_page(
	ppnum_t,
	void*);

#if defined(__arm__) || defined(__arm64__)
/* ARM64_TODO */
extern  bool       pmap_batch_set_cache_attributes(
	upl_page_info_array_t,
	unsigned int,
	unsigned int);
#endif
extern void pmap_sync_page_data_phys(ppnum_t pa);
extern void pmap_sync_page_attributes_phys(ppnum_t pa);

/*
 * debug/assertions. pmap_verify_free returns true iff
 * the given physical page is mapped into no pmap.
 * pmap_assert_free() will panic() if pn is not free.
 */
extern bool pmap_verify_free(ppnum_t pn);
#if MACH_ASSERT
extern void pmap_assert_free(ppnum_t pn);
#endif


/*
 *	Sundry required (internal) routines
 */
#ifdef CURRENTLY_UNUSED_AND_UNTESTED
extern void             pmap_collect(pmap_t pmap);/* Perform garbage
                                                   * collection, if any */
#endif
/*
 *	Optional routines
 */
extern void(pmap_copy)(                         /* Copy range of mappings,
                                                 * if desired. */
	pmap_t          dest,
	pmap_t          source,
	vm_map_offset_t dest_va,
	vm_map_size_t   size,
	vm_map_offset_t source_va);

extern kern_return_t(pmap_attribute)(           /* Get/Set special memory
                                                 * attributes */
	pmap_t          pmap,
	vm_map_offset_t va,
	vm_map_size_t   size,
	vm_machine_attribute_t  attribute,
	vm_machine_attribute_val_t* value);

/*
 * Routines defined as macros.
 */
#ifndef PMAP_ACTIVATE_USER
#ifndef PMAP_ACTIVATE
#define PMAP_ACTIVATE_USER(thr, cpu)
#else   /* PMAP_ACTIVATE */
#define PMAP_ACTIVATE_USER(thr, cpu) {                  \
	pmap_t  pmap;                                           \
                                                                \
	pmap = (thr)->map->pmap;                                \
	if (pmap != pmap_kernel())                              \
	        PMAP_ACTIVATE(pmap, (thr), (cpu));              \
}
#endif  /* PMAP_ACTIVATE */
#endif  /* PMAP_ACTIVATE_USER */

#ifndef PMAP_DEACTIVATE_USER
#ifndef PMAP_DEACTIVATE
#define PMAP_DEACTIVATE_USER(thr, cpu)
#else   /* PMAP_DEACTIVATE */
#define PMAP_DEACTIVATE_USER(thr, cpu) {                        \
	pmap_t  pmap;                                           \
                                                                \
	pmap = (thr)->map->pmap;                                \
	if ((pmap) != pmap_kernel())                    \
	        PMAP_DEACTIVATE(pmap, (thr), (cpu));    \
}
#endif  /* PMAP_DEACTIVATE */
#endif  /* PMAP_DEACTIVATE_USER */

#ifndef PMAP_ACTIVATE_KERNEL
#ifndef PMAP_ACTIVATE
#define PMAP_ACTIVATE_KERNEL(cpu)
#else   /* PMAP_ACTIVATE */
#define PMAP_ACTIVATE_KERNEL(cpu)                       \
	        PMAP_ACTIVATE(pmap_kernel(), THREAD_NULL, cpu)
#endif  /* PMAP_ACTIVATE */
#endif  /* PMAP_ACTIVATE_KERNEL */

#ifndef PMAP_DEACTIVATE_KERNEL
#ifndef PMAP_DEACTIVATE
#define PMAP_DEACTIVATE_KERNEL(cpu)
#else   /* PMAP_DEACTIVATE */
#define PMAP_DEACTIVATE_KERNEL(cpu)                     \
	        PMAP_DEACTIVATE(pmap_kernel(), THREAD_NULL, cpu)
#endif  /* PMAP_DEACTIVATE */
#endif  /* PMAP_DEACTIVATE_KERNEL */

#ifndef PMAP_ENTER
/*
 *	Macro to be used in place of pmap_enter()
 */
#define PMAP_ENTER(pmap, virtual_address, page, protection, fault_type, \
	    flags, wired, result)                                \
	MACRO_BEGIN                                                     \
	pmap_t		__pmap = (pmap);                                \
	vm_page_t	__page = (page);                                \
	int		__options = 0;                                  \
	vm_object_t	__obj;                                          \
                                                                        \
	PMAP_ENTER_CHECK(__pmap, __page)                                \
	__obj = VM_PAGE_OBJECT(__page);                                 \
	if (__obj->internal) {                                          \
	        __options |= PMAP_OPTIONS_INTERNAL;                     \
	}                                                               \
	if (__page->vmp_reusable || __obj->all_reusable) {              \
	        __options |= PMAP_OPTIONS_REUSABLE;                     \
	}                                                               \
	result = pmap_enter_options(__pmap,                             \
	                            (virtual_address),                  \
	                            VM_PAGE_GET_PHYS_PAGE(__page),      \
	                            (protection),                               \
	                            (fault_type),                               \
	                            (flags),                            \
	                            (wired),                            \
	                            __options,                          \
	                            NULL);                              \
	MACRO_END
#endif  /* !PMAP_ENTER */

#ifndef PMAP_ENTER_OPTIONS
#define PMAP_ENTER_OPTIONS(pmap, virtual_address, fault_phys_offset,   \
	    page, protection,                                           \
	    fault_type, flags, wired, options, result)                  \
	MACRO_BEGIN                                                     \
	pmap_t		__pmap = (pmap);                                \
	vm_page_t	__page = (page);                                \
	int		__extra_options = 0;                            \
	vm_object_t	__obj;                                          \
                                                                        \
	PMAP_ENTER_CHECK(__pmap, __page)                                \
	__obj = VM_PAGE_OBJECT(__page);                                 \
	if (__obj->internal) {                                          \
	        __extra_options |= PMAP_OPTIONS_INTERNAL;               \
	}                                                               \
	if (__page->vmp_reusable || __obj->all_reusable) {              \
	        __extra_options |= PMAP_OPTIONS_REUSABLE;               \
	}                                                               \
	result = pmap_enter_options_addr(__pmap,                        \
	                            (virtual_address),                  \
	                            (((pmap_paddr_t)                    \
	                              VM_PAGE_GET_PHYS_PAGE(__page)     \
	                              << PAGE_SHIFT)                    \
	                             + fault_phys_offset),             \
	                            (protection),                       \
	                            (fault_type),                       \
	                            (flags),                            \
	                            (wired),                            \
	                            (options) | __extra_options,        \
	                            NULL);                              \
	MACRO_END
#endif  /* !PMAP_ENTER_OPTIONS */

#ifndef PMAP_SET_CACHE_ATTR
#define PMAP_SET_CACHE_ATTR(mem, object, cache_attr, batch_pmap_op)             \
	MACRO_BEGIN                                                             \
	        if (!batch_pmap_op) {                                           \
	                pmap_set_cache_attributes(VM_PAGE_GET_PHYS_PAGE(mem), cache_attr); \
	                object->set_cache_attr = TRUE;                          \
	        }                                                               \
	MACRO_END
#endif  /* PMAP_SET_CACHE_ATTR */

#ifndef PMAP_BATCH_SET_CACHE_ATTR
#if     defined(__arm__) || defined(__arm64__)
#define PMAP_BATCH_SET_CACHE_ATTR(object, user_page_list,                   \
	    cache_attr, num_pages, batch_pmap_op)                               \
	MACRO_BEGIN                                                             \
	        if ((batch_pmap_op)) {                                          \
	                (void)pmap_batch_set_cache_attributes(                  \
	                                (user_page_list),                       \
	                                (num_pages),                            \
	                                (cache_attr));                          \
	                (object)->set_cache_attr = TRUE;                        \
	        }                                                               \
	MACRO_END
#else
#define PMAP_BATCH_SET_CACHE_ATTR(object, user_page_list,                   \
	    cache_attr, num_pages, batch_pmap_op)                               \
	MACRO_BEGIN                                                             \
	        if ((batch_pmap_op)) {                                          \
	                unsigned int __page_idx=0;                              \
	                while (__page_idx < (num_pages)) {                      \
	                        pmap_set_cache_attributes(                      \
	                                user_page_list[__page_idx].phys_addr,   \
	                                (cache_attr));                          \
	                        __page_idx++;                                   \
	                }                                                       \
	                (object)->set_cache_attr = TRUE;                        \
	        }                                                               \
	MACRO_END
#endif
#endif  /* PMAP_BATCH_SET_CACHE_ATTR */

#define PMAP_ENTER_CHECK(pmap, page)                                    \
{                                                                       \
	if (VMP_ERROR_GET(page)) {                                      \
	        panic("VM page %p should not have an error\n",          \
	                (page));                                        \
	}                                                               \
}

/*
 *	Routines to manage reference/modify bits based on
 *	physical addresses, simulating them if not provided
 *	by the hardware.
 */
struct pfc {
	long    pfc_cpus;
	long    pfc_invalid_global;
};

typedef struct pfc      pmap_flush_context;

/* Clear reference bit */
extern void             pmap_clear_reference(ppnum_t     pn);
/* Return reference bit */
extern boolean_t(pmap_is_referenced)(ppnum_t     pn);
/* Set modify bit */
extern void             pmap_set_modify(ppnum_t  pn);
/* Clear modify bit */
extern void             pmap_clear_modify(ppnum_t pn);
/* Return modify bit */
extern boolean_t        pmap_is_modified(ppnum_t pn);
/* Return modified and referenced bits */
extern unsigned int pmap_get_refmod(ppnum_t pn);
/* Clear modified and referenced bits */
extern void                     pmap_clear_refmod(ppnum_t pn, unsigned int mask);
#define VM_MEM_MODIFIED         0x01    /* Modified bit */
#define VM_MEM_REFERENCED       0x02    /* Referenced bit */
extern void                     pmap_clear_refmod_options(ppnum_t pn, unsigned int mask, unsigned int options, void *);

/*
 * Clears the reference and/or modified bits on a range of virtually
 * contiguous pages.
 * It returns true if the operation succeeded. If it returns false,
 * nothing has been modified.
 * This operation is only supported on some platforms, so callers MUST
 * handle the case where it returns false.
 */
extern bool
pmap_clear_refmod_range_options(
	pmap_t pmap,
	vm_map_address_t start,
	vm_map_address_t end,
	unsigned int mask,
	unsigned int options);


extern void pmap_flush_context_init(pmap_flush_context *);
extern void pmap_flush(pmap_flush_context *);

/*
 *	Routines that operate on ranges of virtual addresses.
 */
extern void             pmap_protect(   /* Change protections. */
	pmap_t          map,
	vm_map_offset_t s,
	vm_map_offset_t e,
	vm_prot_t       prot);

extern void             pmap_protect_options(   /* Change protections. */
	pmap_t          map,
	vm_map_offset_t s,
	vm_map_offset_t e,
	vm_prot_t       prot,
	unsigned int    options,
	void            *arg);

extern void(pmap_pageable)(
	pmap_t          pmap,
	vm_map_offset_t start,
	vm_map_offset_t end,
	boolean_t       pageable);

extern uint64_t pmap_shared_region_size_min(pmap_t map);

extern kern_return_t pmap_nest(pmap_t,
    pmap_t,
    addr64_t,
    uint64_t);
extern kern_return_t pmap_unnest(pmap_t,
    addr64_t,
    uint64_t);

#define PMAP_UNNEST_CLEAN       1

#if __arm64__
#define PMAP_FORK_NEST 1
extern kern_return_t pmap_fork_nest(
	pmap_t old_pmap,
	pmap_t new_pmap,
	vm_map_offset_t *nesting_start,
	vm_map_offset_t *nesting_end);
#endif /* __arm64__ */

extern kern_return_t pmap_unnest_options(pmap_t,
    addr64_t,
    uint64_t,
    unsigned int);
extern boolean_t pmap_adjust_unnest_parameters(pmap_t, vm_map_offset_t *, vm_map_offset_t *);
extern void             pmap_advise_pagezero_range(pmap_t, uint64_t);
#endif  /* MACH_KERNEL_PRIVATE */

extern boolean_t        pmap_is_noencrypt(ppnum_t);
extern void             pmap_set_noencrypt(ppnum_t pn);
extern void             pmap_clear_noencrypt(ppnum_t pn);

/*
 * JMM - This portion is exported to other kernel components right now,
 * but will be pulled back in the future when the needed functionality
 * is provided in a cleaner manner.
 */

extern const pmap_t     kernel_pmap;            /* The kernel's map */
#define pmap_kernel()   (kernel_pmap)

#define VM_MEM_SUPERPAGE        0x100           /* map a superpage instead of a base page */
#define VM_MEM_STACK            0x200

/* N.B. These use the same numerical space as the PMAP_EXPAND_OPTIONS
 * definitions in i386/pmap_internal.h
 */
#define PMAP_CREATE_64BIT          0x1

#if __x86_64__

#define PMAP_CREATE_EPT            0x2
#define PMAP_CREATE_KNOWN_FLAGS (PMAP_CREATE_64BIT | PMAP_CREATE_EPT)

#else

#define PMAP_CREATE_STAGE2         0
#if __arm64e__
#define PMAP_CREATE_DISABLE_JOP    0x4
#else
#define PMAP_CREATE_DISABLE_JOP    0
#endif
#if __ARM_MIXED_PAGE_SIZE__
#define PMAP_CREATE_FORCE_4K_PAGES 0x8
#else
#define PMAP_CREATE_FORCE_4K_PAGES 0
#endif /* __ARM_MIXED_PAGE_SIZE__ */
#define PMAP_CREATE_X86_64         0
#if CONFIG_ROSETTA
#define PMAP_CREATE_ROSETTA        0x20
#else
#define PMAP_CREATE_ROSETTA        0
#endif /* CONFIG_ROSETTA */

/* Define PMAP_CREATE_KNOWN_FLAGS in terms of optional flags */
#define PMAP_CREATE_KNOWN_FLAGS (PMAP_CREATE_64BIT | PMAP_CREATE_STAGE2 | PMAP_CREATE_DISABLE_JOP | PMAP_CREATE_FORCE_4K_PAGES | PMAP_CREATE_X86_64 | PMAP_CREATE_ROSETTA)

#endif /* __x86_64__ */

#define PMAP_OPTIONS_NOWAIT     0x1             /* don't block, return
	                                         * KERN_RESOURCE_SHORTAGE
	                                         * instead */
#define PMAP_OPTIONS_NOENTER    0x2             /* expand pmap if needed
	                                         * but don't enter mapping
	                                         */
#define PMAP_OPTIONS_COMPRESSOR 0x4             /* credit the compressor for
	                                         * this operation */
#define PMAP_OPTIONS_INTERNAL   0x8             /* page from internal object */
#define PMAP_OPTIONS_REUSABLE   0x10            /* page is "reusable" */
#define PMAP_OPTIONS_NOFLUSH    0x20            /* delay flushing of pmap */
#define PMAP_OPTIONS_NOREFMOD   0x40            /* don't need ref/mod on disconnect */
#define PMAP_OPTIONS_ALT_ACCT   0x80            /* use alternate accounting scheme for page */
#define PMAP_OPTIONS_REMOVE     0x100           /* removing a mapping */
#define PMAP_OPTIONS_SET_REUSABLE   0x200       /* page is now "reusable" */
#define PMAP_OPTIONS_CLEAR_REUSABLE 0x400       /* page no longer "reusable" */
#define PMAP_OPTIONS_COMPRESSOR_IFF_MODIFIED 0x800 /* credit the compressor
	                                            * iff page was modified */
#define PMAP_OPTIONS_PROTECT_IMMEDIATE 0x1000   /* allow protections to be
	                                         * be upgraded */
#define PMAP_OPTIONS_CLEAR_WRITE 0x2000
#define PMAP_OPTIONS_TRANSLATED_ALLOW_EXECUTE 0x4000 /* Honor execute for translated processes */
#if defined(__arm__) || defined(__arm64__)
#define PMAP_OPTIONS_FF_LOCKED  0x8000
#define PMAP_OPTIONS_FF_WIRED   0x10000
#endif

#define PMAP_OPTIONS_MAP_TPRO 0x40000

#if     !defined(__LP64__)
extern vm_offset_t      pmap_extract(pmap_t pmap,
    vm_map_offset_t va);
#endif
extern void             pmap_change_wiring(     /* Specify pageability */
	pmap_t          pmap,
	vm_map_offset_t va,
	boolean_t       wired);

/* LP64todo - switch to vm_map_offset_t when it grows */
extern void             pmap_remove(    /* Remove mappings. */
	pmap_t          map,
	vm_map_offset_t s,
	vm_map_offset_t e);

extern void             pmap_remove_options(    /* Remove mappings. */
	pmap_t          map,
	vm_map_offset_t s,
	vm_map_offset_t e,
	int             options);

extern void             fillPage(ppnum_t pa, unsigned int fill);

#if defined(__LP64__)
extern void pmap_pre_expand(pmap_t pmap, vm_map_offset_t vaddr);
extern kern_return_t pmap_pre_expand_large(pmap_t pmap, vm_map_offset_t vaddr);
extern vm_size_t pmap_query_pagesize(pmap_t map, vm_map_offset_t vaddr);
#endif

mach_vm_size_t pmap_query_resident(pmap_t pmap,
    vm_map_offset_t s,
    vm_map_offset_t e,
    mach_vm_size_t *compressed_bytes_p);

extern void pmap_set_vm_map_cs_enforced(pmap_t pmap, bool new_value);
extern bool pmap_get_vm_map_cs_enforced(pmap_t pmap);

/* Inform the pmap layer that there is a JIT entry in this map. */
extern void pmap_set_jit_entitled(pmap_t pmap);

/* Ask the pmap layer if there is a JIT entry in this map. */
extern bool pmap_get_jit_entitled(pmap_t pmap);

/* Inform the pmap layer that the XO register is repurposed for this map */
extern void pmap_set_tpro(pmap_t pmap);

/* Ask the pmap layer if there is a TPRO entry in this map. */
extern bool pmap_get_tpro(pmap_t pmap);

/*
 * Tell the pmap layer what range within the nested region the VM intends to
 * use.
 */
extern void pmap_trim(pmap_t grand, pmap_t subord, addr64_t vstart, uint64_t size);

/*
 * Dump page table contents into the specified buffer.  Returns KERN_INSUFFICIENT_BUFFER_SIZE
 * if insufficient space, KERN_NOT_SUPPORTED if unsupported in the current configuration.
 * This is expected to only be called from kernel debugger context,
 * so synchronization is not required.
 */

extern kern_return_t pmap_dump_page_tables(pmap_t pmap, void *bufp, void *buf_end, unsigned int level_mask, size_t *bytes_copied);

/*
 * Indicates if any special policy is applied to this protection by the pmap
 * layer.
 */
bool pmap_has_prot_policy(pmap_t pmap, bool translated_allow_execute, vm_prot_t prot);

/*
 * Causes the pmap to return any available pages that it can return cheaply to
 * the VM.
 */
uint64_t pmap_release_pages_fast(void);

#define PMAP_QUERY_PAGE_PRESENT                 0x01
#define PMAP_QUERY_PAGE_REUSABLE                0x02
#define PMAP_QUERY_PAGE_INTERNAL                0x04
#define PMAP_QUERY_PAGE_ALTACCT                 0x08
#define PMAP_QUERY_PAGE_COMPRESSED              0x10
#define PMAP_QUERY_PAGE_COMPRESSED_ALTACCT      0x20
extern kern_return_t pmap_query_page_info(
	pmap_t          pmap,
	vm_map_offset_t va,
	int             *disp);

extern uint32_t pmap_lookup_in_static_trust_cache(const uint8_t cdhash[CS_CDHASH_LEN]);
extern bool pmap_lookup_in_loaded_trust_caches(const uint8_t cdhash[CS_CDHASH_LEN]);

extern void pmap_set_compilation_service_cdhash(const uint8_t cdhash[CS_CDHASH_LEN]);
extern bool pmap_match_compilation_service_cdhash(const uint8_t cdhash[CS_CDHASH_LEN]);

extern bool pmap_in_ppl(void);
extern bool pmap_has_ppl(void);

/**
 * Indicates whether the device supports register-level MMIO access control.
 *
 * @note Unlike the pmap-io-ranges mechanism, which enforces PPL-only register
 *       writability at page granularity, this mechanism allows specific registers
 *       on a read-mostly page to be written using a dedicated guarded mode trap
 *       without requiring a full PPL driver extension.
 *
 * @return True if the device supports register-level MMIO access control.
 */
extern bool pmap_has_iofilter_protected_write(void);

/**
 * Performs a write to the I/O register specified by addr on supported devices.
 *
 * @note On supported devices (determined by pmap_has_iofilter_protected_write()), this
 *       function goes over the sorted I/O filter entry table. If there is a hit, the
 *       write is performed from Guarded Mode. Otherwise, the write is performed from
 *       Normal Mode (kernel mode). Note that you can still hit an exception if the
 *       register is owned by PPL but not allowed by an io-filter-entry in the device tree.
 *
 * @note On unsupported devices, this function will panic.
 *
 * @param addr The address of the register.
 * @param value The value to be written.
 * @param width The width of the I/O register, supported values are 1, 2, 4 and 8.
 */
extern void pmap_iofilter_protected_write(vm_address_t addr, uint64_t value, uint64_t width);

extern void *pmap_claim_reserved_ppl_page(void);
extern void pmap_free_reserved_ppl_page(void *kva);

extern void pmap_ledger_verify_size(size_t);
extern ledger_t pmap_ledger_alloc(void);
extern void pmap_ledger_free(ledger_t);

extern bool pmap_is_bad_ram(ppnum_t ppn);
extern kern_return_t pmap_cs_allow_invalid(pmap_t pmap);

#if __arm64__
extern bool pmap_is_exotic(pmap_t pmap);
#else /* __arm64__ */
#define pmap_is_exotic(pmap) false
#endif /* __arm64__ */

extern bool pmap_cs_enabled(void);


/*
 * Returns a subset of pmap_cs non-default configuration,
 * e.g. loosening up of some restrictions through pmap_cs or amfi
 * boot-args. The return value is a bit field with possible bits
 * described below. If default, the function will return 0. Note that
 * this does not work the other way: 0 does not imply that pmap_cs
 * runs in default configuration, and only a small configuration
 * subset is returned by this function.
 *
 * Never assume the system is "secure" if this returns 0.
 */

extern int pmap_cs_configuration(void);

extern kern_return_t pmap_cs_fork_prepare(
	pmap_t old_pmap,
	pmap_t new_pmap
	);

/*
 * The PMAP layer is responsible for holding on to the local signing key so that
 * we can re-use the code for multiple different layers. By keeping our local
 * signing public key here, we can safeguard it with PMAP_CS, and also use it
 * within PMAP_CS for validation.
 *
 * Moreover, we present an API which can be used by AMFI to query the key when
 * it needs to.
 */
#define PMAP_ECC_P384_PUBLIC_KEY_SIZE 97
extern void pmap_set_local_signing_public_key(
	const uint8_t public_key[PMAP_ECC_P384_PUBLIC_KEY_SIZE]
	);

extern uint8_t *pmap_get_local_signing_public_key(void);

/*
 * We require AMFI call into the PMAP layer to unrestrict a particular CDHash
 * for local signing. This only needs to happen for arm devices since x86 devices
 * don't have PMAP_CS.
 *
 * For now, we make the configuration available for x86 devices as well. When
 * AMFI stop calling into this API, we'll remove it.
 */
#define PMAP_SUPPORTS_RESTRICTED_LOCAL_SIGNING 1
extern void pmap_unrestrict_local_signing(
	const uint8_t cdhash[CS_CDHASH_LEN]
	);

#if __has_include(<CoreEntitlements/CoreEntitlements.h>)
/*
 * The PMAP layer provides an API to query entitlements through the CoreEntitlements
 * layer.
 */
extern bool pmap_query_entitlements(
	pmap_t pmap,
	CEQuery_t query,
	size_t queryLength,
	CEQueryContext_t finalContext
	);
#endif

#endif  /* KERNEL_PRIVATE */

#endif  /* _VM_PMAP_H_ */
