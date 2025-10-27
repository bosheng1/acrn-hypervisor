/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vcpu.h>
#include <vm.h>
#include <guest_memory.h>

#include <pgtable.h>

#define INVALID_HPA (0x1UL << 52U)
#define INVALID_GPA (0x1UL << 52U)

int32_t gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	/* TODO: dummy implementation, assuming gva 1-1 mapping to gpa */
	(void)vcpu;
	(void)err_code;
	*gpa = gva;
	return 0;
}

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
	return PAGING_MODE_4_LEVEL;
}

int32_t copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	/* TODO: to be implemented */
	(void)vcpu; (void)h_ptr; (void)gva; (void)size; (void)err_code; (void)fault_addr;
	return 0;
}

int32_t copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	/* TODO: to be implemented */
	(void)vcpu; (void)h_ptr; (void)gva; (void)size; (void)err_code; (void)fault_addr;
	return 0;
}
