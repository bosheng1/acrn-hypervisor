/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stage2.h>
#include <vm.h>

void *get_stg2ptp(struct acrn_vm *vm)
{
	return arch_get_stg2ptp(vm);
}
