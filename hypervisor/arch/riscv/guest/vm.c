/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Yifan Liu <yifan1.liu@intel.com>
 *         Haicheng Li <haicheng.li@intel.com>
 */

#include <vm.h>
#include <vcpu.h>
#include <fdt.h>
#include <vcpu.h>
#include <reloc.h>
#include <cpu.h>
#include <per_cpu.h>
#include <vfdt.h>
#include <libfdt.h>

#include <asm/guest/vcpu_priv.h>

uint32_t vcpu_get_vhartid(struct acrn_vcpu *vcpu)
{
	uint32_t vhartid = vcpu->vcpu_id;

	if (is_service_vm(vcpu->vm)) {
		/* vhartid == phartid */
		vhartid = per_cpu(arch.hart_id, pcpuid_from_vcpu(vcpu));
	}

	/* RISC-V requires that at least one hart has hart ID 0,
	 * so we use logical ID (vcpu_id) for non-service vms.
	 */

	return vhartid;
}

struct acrn_vcpu *vcpu_from_vhartid(struct acrn_vm *vm, uint32_t vhartid)
{
	struct acrn_vcpu *vcpu;

	if (is_service_vm(vm)) {
		/* vhartid == phartid */
		vcpu = vcpu_from_pid(vm, get_pcpu_id_from_hart_id(vhartid));
	} else {
		/* vhartid == vcpu_id */
		vcpu = vcpu_from_vid(vm, vhartid);
	}
	return vcpu;
}

static inline uint64_t calculate_memory_size(struct vm_hpa_regions *regions, uint64_t num)
{
	uint64_t i;
	uint64_t size = 0;
	for(i = 0; i < num; i++) {
		size += regions[i].size_hpa;
	}

	return size;
}

#define SERVICE_VM_MAX_GPA  0x280000000   //0x100000000
/*TODO: hard code memory entry here, need to generate common entry data from virtual memory map, eg. dts */
void arch_prepare_vm_memmap(struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	if (is_service_vm(vm)) {
		uint64_t hv_hpa, hv_size;
		init_service_vm_vfdt(vm);
		register_mmap_entry(vm, 0, SERVICE_VM_MAX_GPA, PAGE_V | PAGE_R | PAGE_W | PAGE_U | PAGE_X, MAP);

		hv_hpa = hva2hpa((void *)(get_hv_image_base()));
		hv_size = get_hv_image_size();
		register_mmap_entry(vm, hv_hpa, hv_size, 0, DELETE);

		for (uint16_t vmid = 0; vmid < CONFIG_MAX_VM_NUM; vmid++) {
			vm_config = get_vm_config(vmid);
			if (vm_config->load_order == PRE_LAUNCHED_VM) {
				for (uint16_t i = 0; i < vm_config->memory.region_num; i++) {

					register_mmap_entry(vm, vm_config->memory.host_regions[i].start_hpa, vm_config->memory.host_regions[i].size_hpa, 0, DELETE);
				}
			}
		}

	} else if (vm_config->load_order == PRE_LAUNCHED_VM) {
		uint64_t memory_size = calculate_memory_size(vm_config->memory.host_regions, vm_config->memory.region_num);
		register_mmap_entry(vm, 0, memory_size, PAGE_V | PAGE_R | PAGE_W | PAGE_U | PAGE_X, MAP);

	} else if (vm_config->load_order == POST_LAUNCHED_VM) {

	}
}

int32_t arch_init_vm(struct acrn_vm *vm, struct acrn_vm_config *vm_config)
{
	init_vsbi(vm);
	arch_prepare_vm_memmap(vm);
	create_vm_memmap(vm);
	(void)vm_config;
	init_legacy_vuarts(vm, vm_config->vuart);
	vplic_init(vm);

	return 0;
}

int32_t arch_deinit_vm(struct acrn_vm *vm)
{
	(void)vm;
	return 0;
}

int32_t arch_reset_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu);
	}
	vplic_reset(vm);
	return 0;
}

void arch_vm_prepare_bsp(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;

	vcpu_set_epc(vcpu, (uint64_t)vm->sw.kernel_info.kernel_entry_addr);

	vcpu->arch.regs.a0 = vcpu_get_vhartid(vcpu);
	vcpu->arch.regs.a1 = (uint64_t)vm->sw.fdt_info.load_addr;
}

void arch_trigger_level_intr(struct acrn_vm *vm, uint32_t irq, bool assert)
{
	vplic_accept_intr(vm, irq, assert);
}

static void fdt_set_hart_isa_str_all(void *fdt, const char *isa_str)
{
	int cpus_off, cpu, ret, len;
	const char *val;

	cpus_off = fdt_path_offset(fdt, "/cpus");
	if (cpus_off > 0) {
		fdt_for_each_subnode(cpu, fdt, cpus_off) {
			val = (const char *)fdt_getprop(fdt, cpu, "device_type", &len);
			if ((len > 0) && (strncmp(val, "cpu", 3) == 0)) {
				ret = fdt_setprop_string(fdt, cpu, "riscv,isa", isa_str);
				if (ret < 0) {
					pr_err("Failed to set hart isa string: %d", ret);
				}
			}
		}
	}

}

void print_interrupts_property(const void *data, int len) {
    const fdt32_t *interrupts = (const fdt32_t *)data;
    int cells = len / 4;

    printf("interrupts = <");
    for (int i = 0; i < cells; i++) {
        if (i > 0) printf(" ");
        printf("%d", fdt32_to_cpu(interrupts[i]));
    }
    printf(">\n");
}
void print_interrupts_parent_property(const void *data, int len) {
    const fdt32_t *interrupts = (const fdt32_t *)data;
    int cells = len / 4;

    printf("interrupts parent = <");
    for (int i = 0; i < cells; i++) {
        if (i > 0) printf(" ");
        printf("%d", fdt32_to_cpu(interrupts[i]));
    }
    printf(">\n");
}

void print_device_properties(void *fdt, int node) {
    int prop_offset = 0;
    const char *prop_name;
    const void *prop_data;
    int prop_len;

    fdt_for_each_property_offset(prop_offset, fdt, node) {
        prop_data = fdt_getprop_by_offset(fdt, prop_offset, &prop_name, &prop_len);

        if (!prop_data || !prop_name) continue;

        pr_err("  %s: ", prop_name);

        if (strcmp(prop_name, "compatible") == 0) {
           pr_err("device compatible:%s\n", prop_data); 
	}
        if (strcmp(prop_name, "interrupts") == 0) {
			print_interrupts_property(prop_data, prop_len);
	}
        if (strcmp(prop_name, "interrupt-parent") == 0) {
			print_interrupts_parent_property(prop_data, prop_len);
	}
    }
}
void arch_init_service_vm_vfdt(struct acrn_vm *vm)
{
	/* TODO: For now hardcode the isa string.
	 *
	 * To do it formally, get isa string from host, remove extensions
	 * that we do not support (such as "h") and pass to vm.
	 */
	void *fdt = vm->arch_vm.fdt_raw;
	const char *isa_str = "rv64imafdc_zicsr_zifencei_sstc";
	fdt_set_hart_isa_str_all(vm->arch_vm.fdt_raw, isa_str);
	int node = 0;
	node = fdt_node_offset_by_compatible(fdt, -1, "riscv,plic0");
	const uint32_t * phandle_prop = fdt_getprop(fdt, node, "phandle", NULL);
	uint32_t phandle = *phandle_prop;
	int parent_node, serial_node;
	parent_node = fdt_path_offset(fdt, "/soc");
	serial_node = fdt_add_subnode(fdt, parent_node, "serial@10000000");
	if (serial_node >0) {
		pr_err(" create uart dt\n");
		fdt_setprop_cell(fdt, serial_node, "interrupts", 10);
		fdt_setprop_cell(fdt, serial_node, "interrupt-parent", cpu_to_fdt32(phandle));
		uint32_t clock_freq = 3686400;
		fdt_setprop_cell(fdt, serial_node, "clock-frequency", clock_freq);
		fdt_setprop_string(fdt, serial_node, "compatible", "ns16550a");
		uint32_t reg_prop[4];
		reg_prop[0] = cpu_to_fdt32((uint32_t)(0));
		reg_prop[1] = cpu_to_fdt32((uint32_t)(0x10000000));
		reg_prop[2] = cpu_to_fdt32((uint32_t)(0));
		reg_prop[3] = cpu_to_fdt32((uint32_t)(0x100));
		fdt_setprop(fdt, serial_node, "reg", reg_prop, sizeof(reg_prop));

	}
	int child_node = 0;
	fdt_for_each_subnode(child_node, fdt, parent_node) {
		const char *name = fdt_get_name(fdt, child_node, NULL);
	        pr_err("Device: %s (offset: %d)\n", name ? name : "unknown", child_node);
		print_device_properties(fdt, child_node);
	}
}
