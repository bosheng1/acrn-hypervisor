/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <guest_memory.h>
#include <pgtable.h>
#include <stg2_mm.h>
#include <vm.h>
#include <mmu.h>
#include <logmsg.h>
#include <cpu.h>

/**
 * @pre: vm != NULL.
 */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size)
{
	/* using return value INVALID_HPA as error code */
	uint64_t hpa = INVALID_HPA;
	const uint64_t *pgentry;
	uint64_t pg_size = 0UL;
	void *stg2ptp;

	stg2ptp = get_stg2ptp(vm);
	pgentry = pgtable_lookup_entry((uint64_t *)stg2ptp, gpa, &pg_size, &vm->stg2_pgtable);
	if (pgentry != NULL) {
		hpa = (((*pgentry & (~STAGE2_PFN_HIGH_MASK)) & (~(pg_size - 1UL)))
				| (gpa & (pg_size - 1UL)));
	}

	/**
	 * If specified parameter size is not NULL and
	 * the HPA of parameter gpa is found, pg_size shall
	 * be returned through parameter size.
	 */
	if ((size != NULL) && (hpa != INVALID_HPA)) {
		*size = (uint32_t)pg_size;
	}

	return hpa;
}

uint32_t local_copy_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa,
	uint32_t size, uint32_t fix_pg_size, bool cp_from_vm)
{
	uint64_t hpa;
	uint32_t offset_in_pg, len, pg_size;
	void *g_ptr;

	hpa = local_gpa2hpa(vm, gpa, &pg_size);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping",
			__func__, vm->vm_id, gpa);
		len = 0U;
	} else {

		if (fix_pg_size != 0U) {
			pg_size = fix_pg_size;
		}

		offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
		len = (size > (pg_size - offset_in_pg)) ? (pg_size - offset_in_pg) : size;

		g_ptr = hpa2hva(hpa);

		pre_user_access();
		if (cp_from_vm) {
			(void)memcpy_s(h_ptr, len, g_ptr, len);
		} else {
			(void)memcpy_s(g_ptr, len, h_ptr, len);
		}
		post_user_access();
	}

	return len;
}

static inline int32_t copy_gpa(struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg,
	uint32_t size_arg, bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint32_t len;
	uint64_t gpa = gpa_arg;
	uint32_t size = size_arg;
	int32_t err = 0;

	while (size > 0U) {
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		if (len == 0U) {
			err = -EINVAL;
			break;
		}
		gpa += len;
		h_ptr += len;
		size -= len;
	}

	return err;
}
/* using return value INVALID_HPA as error code */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa)
{
	return local_gpa2hpa(vm, gpa, NULL);
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 1);
	if (ret != 0) {
		pr_err("Unable to copy GPA 0x%llx from VM%d to HPA 0x%llx\n", gpa, vm->vm_id, (uint64_t)h_ptr);
	}

	return ret;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 0);
	if (ret != 0) {
		pr_err("Unable to copy HPA 0x%llx to GPA 0x%llx in VM%d\n", (uint64_t)h_ptr, gpa, vm->vm_id);
	}

	return ret;
}

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	uint64_t hpa = gpa2hpa(vm, x);
	return (hpa == INVALID_HPA) ? NULL : hpa2hva(hpa);
}
