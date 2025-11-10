/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STAGE2_H
#define STAGE2_H

#include <vm.h>
void *arch_get_stg2ptp(struct acrn_vm *vm);
void arch_stg2pt_flush_guest(struct acrn_vm *vm);
void arch_init_s2pt(struct pgtable *table, void **s2ptp, struct vm_arch *arch_vm, uint16_t vm_id);
#endif /* STAGE2_H */
