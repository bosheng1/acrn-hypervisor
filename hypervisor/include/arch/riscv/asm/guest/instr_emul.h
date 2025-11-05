/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef __RISCV_INSTR_EMUL_H__
#define __RISCV_INSTR_EMUL_H__

#include <types.h>
#include <asm/cpu.h>

struct acrn_vcpu;

int32_t emulate_instruction(struct acrn_vcpu *vcpu);
int32_t decode_instruction(uint32_t ins);

#endif /* __RISCV_INSTR_EMUL_H__ */
