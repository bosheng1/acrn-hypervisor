/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_IRQ_H
#define RISCV_IRQ_H

#include <cpu.h>
#include <common/vm.h>
#include <dm/io_req.h>

#define IPI_NOTIFY_CPU		0
#define EXCEPTION_INVALID	0x7fffffffffffffffUL

struct intr_excp_ctx {
	struct cpu_regs regs;
};

void init_interrupt(uint16_t pcpu_id);

/* FIXME: this is temporary solution. The hypercall support in risc-v should modify here. */
#define HYPERVISOR_CALLBACK_HSM_VECTOR	0x20U
static inline void arch_fire_hsm_interrupt(void) {}

/* FIXME: this is temporary solution. PIO support in risc-v should modify here. */
void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address, uint32_t nbytes);
void emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req);

#endif /* RISCV_IRQ_H */
