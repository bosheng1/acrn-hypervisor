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
 * @brief EPT page tables destroy
 *
 * @param[inout] vm the pointer that points to VM data structure
 */
void destroy_ept(struct acrn_vm *vm);

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
