/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __X86_MM_COMMON_H__
#define __X86_MM_COMMON_H__
#include <asm/pgtable.h>

#define PGTL3_SHIFT PML4E_SHIFT
#define PGTL3_SIZE (1UL << PGTL3_SHIFT)
#define PGTL3_MASK (~(PGTL3_SIZE - 1UL))
#define PTRS_PER_PGTL3E PTRS_PER_PML4E
#define PGTL2_SHIFT PDPTE_SHIFT
#define PGTL2_SIZE (1UL << PGTL2_SHIFT)
#define PGTL2_MASK (~(PGTL2_SIZE - 1UL))
#define PTRS_PER_PGTL2E PTRS_PER_PDPTE
#define PGTL1_SHIFT PDE_SHIFT
#define PGTL1_SIZE (1UL << PGTL1_SHIFT)
#define PGTL1_MASK (~(PGTL1_SIZE - 1UL))
#define PTRS_PER_PGTL1E PTRS_PER_PDE
#define PGTL0_SHIFT PTE_SHIFT
#define PGTL0_SIZE (1UL << PGTL0_SHIFT)
#define PGTL0_MASK (~(PGTL0_SIZE - 1UL))
#define PTRS_PER_PGTL0E PTRS_PER_PTE
#define PAGE_PFN_OFFSET 12
/**
 * Invalid HPA is defined for error checking,
 * according to SDM vol.3A 4.1.4, the maximum
 * host physical address width is 52
 */
#define INVALID_HPA		(0x1UL << 52U)
#define INVALID_GPA		(0x1UL << 52U)
#define STAGE2_PFN_HIGH_MASK  	EPT_PFN_HIGH_MASK
#endif /*__X86_MM_COMMON_H__ */
