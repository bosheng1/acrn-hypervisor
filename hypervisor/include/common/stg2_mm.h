/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef STAGE2_MM_H
#define STAGE2_MM_H

#include <asm/mm_common.h>
#include <vm.h>

/**
 * @brief Get EPT pointer of the vm
 *
 * @param[in] vm the pointer that points to VM data structure
 *
 * @retval If the current context of vm is SECURE_WORLD, return EPT pointer of
 *            secure world, otherwise return EPT pointer of normal world.
 */
void *get_stg2ptp(struct acrn_vm *vm);

/**
 * @brief Guest-physical memory region mapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] toplvl_page The physical address of The EPTP
 * @param[in] hpa The specified start host physical address of host
 *                physical memory region that GPA will be mapped
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region that needs to be mapped
 * @param[in] size The size of guest physical memory region that needs
 *                 to be mapped
 * @param[in] prot_orig The specified memory access right and memory type
 */
void stg2pt_add_mr(struct acrn_vm *vm, uint64_t *toplvl_page,
	uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig);

/**
 * @brief Guest-physical memory page access right or memory type updating
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] toplvl_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *            physical memory region whoes mapping needs to be updated
 * @param[in] size The size of guest physical memory region
 * @param[in] prot_set The specified memory access right and memory type
 *                     that will be set
 * @param[in] prot_clr The specified memory access right and memory type
 *                     that will be cleared
 */
void stg2pt_modify_mr(struct acrn_vm *vm, uint64_t *toplvl_page,
		uint64_t gpa, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr);

/**
 * @brief Guest-physical memory region unmapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] toplvl_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region whoes mapping needs to be deleted
 * @param[in] size The size of guest physical memory region
 *
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void stg2pt_del_mr(struct acrn_vm *vm, uint64_t *toplvl_page, uint64_t gpa, uint64_t size);

/**
 * @brief Translating from host-physical address to guest-physical address for Service VM
 *
 * @param[in] hpa the specified host-physical address
 *
 * @pre: the gpa and hpa are identical mapping in Service VM.
 */
uint64_t service_vm_hpa2gpa(uint64_t hpa);

/**
 * @brief Check if the GPA range is guest valid GPA or not
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] mr_base_gpa The specified start guest physical address of guest
 *                        physical memory region
 * @param[in] size The size of guest physical memory region
 *
 * @retval true if the GPA range is guest valid GPA, false otherwise.
 */
bool stg2pt_is_valid_mr(struct acrn_vm *vm, uint64_t mr_base_gpa, uint64_t size);

void init_stg2_mm(struct acrn_vm *vm);

#endif /* STAGE2_MM_H */
