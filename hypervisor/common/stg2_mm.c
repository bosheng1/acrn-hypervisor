/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stage2.h>
#include <vm.h>
#include <mmu.h>
#include <logmsg.h>

#define DBG_LEVEL_STG2P	6U

void *get_stg2ptp(struct acrn_vm *vm)
{
	return arch_get_stg2ptp(vm);
}

static void stg2pt_flush_guest(struct acrn_vm *vm)
{
	arch_stg2pt_flush_guest(vm);
}

void stg2pt_add_mr(struct acrn_vm *vm, uint64_t *toplvl_page,
	uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	uint64_t prot = prot_orig;

	dev_dbg(DBG_LEVEL_STG2P, "%s, vm[%d] hpa: 0x%016lx gpa: 0x%016lx size: 0x%016lx prot: 0x%016x\n",
			__func__, vm->vm_id, hpa, gpa, size, prot);

	spinlock_obtain(&vm->stg2pt_lock);

	pgtable_add_map(toplvl_page, hpa, gpa, size, prot, &vm->stg2_pgtable);

	spinlock_release(&vm->stg2pt_lock);

	stg2pt_flush_guest(vm);
}

void stg2pt_modify_mr(struct acrn_vm *vm, uint64_t *toplvl_page,
		uint64_t gpa, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr)
{
	uint64_t local_prot = prot_set;

	dev_dbg(DBG_LEVEL_STG2P, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->stg2pt_lock);

	pgtable_modify_or_del_map(toplvl_page, gpa, size, local_prot, prot_clr, &(vm->stg2_pgtable), MR_MODIFY);

	spinlock_release(&vm->stg2pt_lock);

	stg2pt_flush_guest(vm);
}
/**
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void stg2pt_del_mr(struct acrn_vm *vm, uint64_t *toplvl_page, uint64_t gpa, uint64_t size)
{
	dev_dbg(DBG_LEVEL_STG2P, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->stg2pt_lock);

	pgtable_modify_or_del_map(toplvl_page, gpa, size, 0UL, 0UL, &(vm->stg2_pgtable), MR_DEL);

	spinlock_release(&vm->stg2pt_lock);

	stg2pt_flush_guest(vm);
}
