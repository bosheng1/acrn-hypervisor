/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef __RISCV_VIO_H__
#define __RISCV_VIO_H__

#include <types.h>
#include <common/vcpu.h>

int32_t mmio_inst_fault_handler(struct acrn_vcpu *vcpu);

#endif /* __RISCV_VIO_H__ */
