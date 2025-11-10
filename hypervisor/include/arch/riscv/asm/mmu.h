/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RISCV_MMU_H
#define RISCV_MMU_H

#include <types.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#define MAX_FDT_RSVD_RANGES 16

static inline void set_satp(uint64_t satp)
{
	asm volatile (
		"csrw satp, %0\n\t" \
		"sfence.vma"
		:: "r"(satp)
		: "memory"
	);
}
void init_paging(void);
void enable_paging(void);
enum _page_table_level;
struct pgtable;
uint64_t pgentry_present_helper(uint64_t pte);

bool large_page_support(enum _page_table_level level, uint64_t __unused prot);

void set_pgentry_helper(uint64_t *pte, uint64_t page, uint64_t prot, enum _page_table_level __unused level,
		bool is_leaf, const struct pgtable *table);
#endif /* RISCV_MMU_H */
