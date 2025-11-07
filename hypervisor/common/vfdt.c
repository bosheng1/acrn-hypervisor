/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <reloc.h>
#include <vm_config.h>
#include <libfdt.h>
#include <vfdt.h>
#include <fdt_api.h>

static void init_vm_vfdt_common(struct acrn_vm *vm)
{
	void *fdt = vm->arch_vm.fdt_raw;
	uint64_t ramdisk_start, ramdisk_end;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	if (vm_config->os_config.bootargs[0] != '\0') {
		fdt_set_kernel_bootargs(fdt, vm_config->os_config.bootargs);
	}

	if (vm->sw.ramdisk_info.load_addr != NULL) {
		ramdisk_start = (uint64_t)vm->sw.ramdisk_info.load_addr;
		ramdisk_end = ramdisk_start + (uint64_t)vm->sw.ramdisk_info.size;
		fdt_set_initrd_mem_range(fdt, ramdisk_start, ramdisk_end);
	}

	vm->sw.fdt_info.src_addr = fdt;
	vm->sw.fdt_info.size = fdt_totalsize(fdt);
	/* load addr is initialized in image loader */
}

void init_service_vm_vfdt(struct acrn_vm *vm)
{
	uint8_t *fdt = vm->arch_vm.fdt_raw;
	struct acrn_vm_config *vm_config;
	uint16_t vm_id;
	uint64_t i, addr, size;

	/* Re-use host FDT */
	fdt_move(get_host_fdt(), fdt, MAX_FDT_SIZE);

	/* Reserve hypervisor mem range */
	fdt_add_rsvd_node(fdt, get_hv_image_base(), get_hv_image_size());

	/* Remove hv owned devices */
	/* TODO: to be implemented */
	/* for now remove serial device directly */
	if (fdt_remove_node_by_path(fdt, "/soc/serial") < 0) {
		pr_err("Failed to remove serial device from Service VM vfdt");
	}

	/* Reserve pre-launched VM mem range */
	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (i = 0; i < vm_config->memory.region_num; i++) {
				addr = vm_config->memory.host_regions[i].start_hpa;
				size = vm_config->memory.host_regions[i].size_hpa;
				fdt_add_rsvd_node(fdt, addr, size);
			}
		}
	}

	/* Remove pre-launch owned devices */
	/* TODO: to be implemented */

	arch_init_service_vm_vfdt(vm);

	init_vm_vfdt_common(vm);
}
