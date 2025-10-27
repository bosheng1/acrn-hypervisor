/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EPT_H
#define EPT_H
#include <types.h>
#include <mmu.h>
#include <stg2_mm.h>

typedef void (*pge_handler)(uint64_t *pgentry, uint64_t size);

struct acrn_vm;

/* External Interfaces */
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
bool ept_is_valid_mr(struct acrn_vm *vm, uint64_t mr_base_gpa, uint64_t size);

/**
 * @brief EPT page tables destroy
 *
 * @param[inout] vm the pointer that points to VM data structure
 */
void destroy_ept(struct acrn_vm *vm);
/**
 * @brief Translating from host-physical address to guest-physical address for Service VM
 *
 * @param[in] hpa the specified host-physical address
 *
 * @pre: the gpa and hpa are identical mapping in Service VM.
 */
uint64_t service_vm_hpa2gpa(uint64_t hpa);

/**
 * @brief Flush address space from the page entry
 *
 * @param[in] pge the pointer that points to the page entry
 *
 * @param[in] size the size of the page
 */
void ept_flush_leaf_page(uint64_t *pge, uint64_t size);

/**
 * @brief Walking through EPT table
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] cb the pointer that points to walk_ept_table callback, the callback
 * 		will be invoked when getting a present page entry from EPT, and
 *		the callback could get the page entry and page size parameters.
 */
void walk_ept_table(struct acrn_vm *vm, pge_handler cb);

/**
 * @brief EPT misconfiguration handling
 *
 * @param[in] vcpu the pointer that points to vcpu data structure
 *
 * @retval -EINVAL fail to handle the EPT misconfig
 * @retval 0 Success to handle the EPT misconfig
 */
int32_t ept_misconfig_vmexit_handler(__unused struct acrn_vcpu *vcpu);

void init_ept_pgtable(struct pgtable *table, uint16_t vm_id);
void reserve_buffer_for_ept_pages(void);
#endif /* EPT_H */
