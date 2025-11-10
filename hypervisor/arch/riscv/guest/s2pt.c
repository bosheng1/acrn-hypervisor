/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define S2PT_PAGE_NUM 0x200

#include <vm.h>
#include <mmu.h>

DEFINE_16K_PAGE_TABLES(s2pt_root_pages, CONFIG_MAX_VM_NUM);
DEFINE_PAGE_TABLES(s2pt_pages, S2PT_PAGE_NUM*CONFIG_MAX_VM_NUM);
DEFINE_PAGE_TABLES(s2pt_pages_bitmap, CONFIG_MAX_VM_NUM);

static struct page_pool s2pt_page_pool[CONFIG_MAX_VM_NUM];

static void s2pt_flush_cache_pagewalk(const void* entry __attribute__((unused)))
{
}

void arch_init_s2pt(struct pgtable *table, void **s2ptp, struct vm_arch *arch_vm, uint16_t vm_id)
{
	init_page_pool(&s2pt_page_pool[vm_id], (uint64_t *)&s2pt_pages[vm_id*S2PT_PAGE_NUM],
			(uint64_t *)&s2pt_pages_bitmap[vm_id], S2PT_PAGE_NUM);

	table->pool = &s2pt_page_pool[vm_id];
	table->pgentry_present = pgentry_present_helper;
	table->flush_cache_pagewalk = s2pt_flush_cache_pagewalk;
	table->large_page_support = large_page_support;
	table->set_pgentry = set_pgentry_helper;
	*s2ptp = (uint64_t *)&s2pt_root_pages[vm_id];

	arch_vm->hgatp = SATP_MODE_SV48 | ((uint64_t)vm_id << STAGE2_VMID_OFFSET) | ((uint64_t)*s2ptp >> PAGE_SHIFT);
}
void arch_stg2pt_flush_guest(struct acrn_vm __unused *vm)
{
}

uint64_t arch_get_stage2_pfn(uint64_t pgentry)
{
	return (((pgentry & (~STAGE2_PFN_HIGH_MASK))>>PAGE_PFN_OFFSET)<<PAGE_SHIFT);
}
