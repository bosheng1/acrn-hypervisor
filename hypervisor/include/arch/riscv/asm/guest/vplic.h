/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef __RISCV_VPLIC_H__
#define __RISCV_VPLIC_H__

#include <asm/plicbase.h>
#include <asm/page.h>
#include <spinlock.h>
#include <config.h>

#define HVIP_VSEIP              10U
#define IRQ_S_MODE              9U
#define VPLIC_BASE              0x0C000000U
#define VPLIC_SIZE              0x04000000U
#define VPLIC_MAX_NUM_SOURCE    CONFIG_MAX_PLIC_SOURCES
#define VPLIC_MAX_PRIORITY      7U

#define PLIC_MAX_NUM_FIELDS     ((PLIC_MAX_SOURCES + 31U) / 32U)

struct plic_regs {
        uint32_t source_priority[PLIC_MAX_SOURCES];
        uint32_t pending[PLIC_MAX_NUM_FIELDS];
        uint32_t enable[PLIC_VM_MAX_CONTEXTS][PLIC_MAX_NUM_FIELDS];
        uint32_t threshold[PLIC_VM_MAX_CONTEXTS];
        uint32_t claimed[PLIC_VM_MAX_CONTEXTS];
} __aligned(PAGE_SIZE);

struct acrn_vplic {
	bool enabled;
	spinlock_t lock;
	struct plic_regs regs;
	struct acrn_vm *vm;
	struct plic_info info;
	uint32_t priority_base;
	uint32_t pending_base;
	uint32_t enable_base;
	uint32_t context_base;
	bool asserted[PLIC_VM_MAX_CONTEXTS];
};

void vplic_init(struct acrn_vm *vm);
void vplic_reset(struct acrn_vm *vm);
void vplic_accept_intr(struct acrn_vm *vm, uint32_t irq, bool assert);

#endif /* __RISCV_VPLIC_H__ */
