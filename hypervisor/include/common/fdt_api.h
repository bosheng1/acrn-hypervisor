/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FDT_API_H
#define FDT_API_H
#include <types.h>
#include <libfdt.h>
#include <mmu.h>

#define MAX_FDT_SIZE (64 * MEM_1K)

void init_devtree(uint64_t fdt_paddr);
uint8_t *get_host_fdt(void);

int fdt_get_phys_mem_region(const void *fdt, uint64_t *addr_out, uint64_t *size_out);
int fdt_get_rsvd_mem_regions(const void *fdt, struct mem_range *out_ranges, int *out_nr_range);
int fdt_add_rsvd_node(void *fdt, uint64_t addr, uint64_t size);
int fdt_set_kernel_bootargs(void *fdt, const char *bootargs);
int fdt_set_initrd_mem_range(void *fdt, uint64_t gva_start, uint64_t initrd_size);
int fdt_remove_node_by_path(void *fdt, const char *name);

#endif /* FDT_API_H */
