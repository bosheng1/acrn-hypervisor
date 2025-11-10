/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define S2PT_PAGE_NUM 0x200

#include <vm.h>
#include <mmu.h>

DEFINE_16K_PAGE_TABLES(s2pt_root_pages, CONFIG_MAX_VM_NUM);
DEFINE_PAGE_TABLES(s2pt_pages, S2PT_PAGE_NUM*CONFIG_MAX_VM_NUM);
DEFINE_PAGE_TABLES(s2pt_pages_bitmap, CONFIG_MAX_VM_NUM);
