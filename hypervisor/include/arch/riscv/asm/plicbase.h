/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __RISCV_BASE_H__
#define __RISCV_BASE_H__
#include <types.h>
#include <logmsg.h>
#include <asm/page.h>

/* PLIC base definitions, refer to riscv-plic-1.0.0.pdf */
#define PLIC_SRC_PRIO_BASE      0x0U
#define PLIC_PENDING_BASE       0x1000U
#define PLIC_ENABLE_BASE        0x2000U
#define PLIC_ENABLE_STRIDE      0x80U
#define PLIC_CONTEXT_BASE       0x200000U
#define PLIC_THRESHOLD_BASE     0x0U
#define PLIC_EOI_BASE           0x4U
#define PLIC_CONTEXT_STRIDE     0x1000U
#define PLIC_MAX_SOURCES        1024U
#define PLIC_MAX_CONTEXTS       15872U

/* Each hart supports machine mode and supervisor mode, so max contexts is 2 * MAX_PCPU_NUM */
#define PLIC_VM_MAX_CONTEXTS    (2U * MAX_PCPU_NUM)

struct context_info {
        uint32_t hartid; /* target cpu */
        uint32_t parent_hwirq; /* parent hw irq, 9 for hypervisor mode, 11 for machine mode */
};

struct plic_info {
        uint64_t base;
        uint64_t size;
        uint32_t max_priority;
        uint32_t source_num;
        uint32_t context_num;
        struct context_info contexts[PLIC_VM_MAX_CONTEXTS];
};
#endif /* __RISCV_BASE_H__ */
