/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 *   Haibo1 Xu <haibo1.xu@intel.com>
 *   Bosheng Xue <bosheng.xue@intel.com>
 */

#define pr_prefix	"vplic: "

#include <types.h>
#include <errno.h>
#include <pgtable.h>
#include <irq.h>
#include <stg2_mm.h>
#include <vm.h>
#include <logmsg.h>
#include <asm/guest/vplic.h>
#include <asm/guest/virq.h>
#include <fdt_api.h>

#define VPLIC_VERBOS	0
#define DBG_LEVEL_VPLIC	6U

static uint32_t vplic_num_fields(const struct acrn_vplic *vplic)
{
	return (vplic->info.source_num + 31U) / 32U;
}

#if VPLIC_VERBOS
static inline void vplic_dump_regs(const struct acrn_vplic *vplic)
{
	const struct plic_regs *regs = &vplic->regs;
	uint32_t i, j;

	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC VMID:%d, Start Reg Dump\n", vplic->vm->vm_id);
	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC Source Priority Reg:\n");
	for (i = 0U; i <= vplic->info.source_num; i++) {
		dev_dbg(DBG_LEVEL_VPLIC, "0x%04x ", regs->source_priority[i]);
	}

	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC Pending Reg:\n");
	for (i = 0U; i < vplic_num_fields(vplic); i++) {
		dev_dbg(DBG_LEVEL_VPLIC, "0x%04x ", regs->pending[i]);
	}

	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC Enable Reg:\n");
	for (i = 0U; i < vplic->info.context_num; i++) {
		dev_dbg(DBG_LEVEL_VPLIC, "Context %d:\n", i);
                for (j = 0; j < vplic_num_fields(vplic); j++) {
			dev_dbg(DBG_LEVEL_VPLIC, "0x%04x ", regs->enable[i][j]);
		}
	}

	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC context Reg:\n");
        for (i = 0U; i < vplic->info.context_num; i++) {
                dev_dbg(DBG_LEVEL_VPLIC, "Context %d:\n", i);
                dev_dbg(DBG_LEVEL_VPLIC, "0x%04x, 0x%04x ", regs->threshold[i], regs->claimed[i]);
        }
	dev_dbg(DBG_LEVEL_VPLIC, "VPLIC Finish Reg Dump\n");
}
#else
static inline void vplic_dump_regs(__unused const struct acrn_vplic *vplic) {}
#endif

static void vplic_reg_set_bit(volatile void *p, uint32_t nr)
{
	*(uint32_t *)p |= (1U << nr);
}

static void vplic_reg_clear_bit(volatile void *p, uint32_t nr)
{
	*(uint32_t *)p &= ~(1U << nr);
}

static void vplic_set_pending(struct plic_regs *regs, uint32_t irq)
{
        vplic_reg_set_bit(&regs->pending[irq >> 5U], irq & 31U);
}

static void vplic_clear_pending(struct plic_regs *regs, uint32_t irq)
{
        vplic_reg_clear_bit(&regs->pending[irq >> 5U], irq & 31U);
}

static void vplic_set_claimed(struct plic_regs *regs, uint32_t context_id, uint32_t irq)
{
        vplic_reg_set_bit(&regs->claimed[context_id], irq);
}

static void vplic_clear_claimed(struct plic_regs *regs, uint32_t context_id, uint32_t irq)
{
        vplic_reg_clear_bit(&regs->claimed[context_id], irq);
}

static uint32_t vplic_get_deliverable_irq(struct acrn_vplic *vplic, uint32_t context_id)
{
	uint32_t max_irq = 0U;
	struct plic_regs *regs = &vplic->regs;
	uint32_t max_prio = regs->threshold[context_id];

	for (uint32_t i = 0U; i < vplic_num_fields(vplic); i++) {
		uint32_t irq_candidate = (regs->pending[i] & ~regs->claimed[context_id]) &
							regs->enable[context_id][i];

		if (!irq_candidate)
			continue;

		for (uint32_t j = 0U; j < 32U; j++) {
			uint32_t irq = (i << 5U) + j;
			uint32_t prio = regs->source_priority[irq];
			bool enabled = !!(irq_candidate & (1U << j));

			if (enabled && prio > max_prio) {
				max_irq = irq;
				max_prio = prio;
			}
		}
	}

	return max_irq;
}

static bool check_context_s_mode(struct acrn_vplic *vplic, uint32_t context_id)
{
	return (vplic->info.contexts[context_id].parent_hwirq == IRQ_S_MODE);
}

static void vplic_vcpu_intr_assert(struct acrn_vcpu *vcpu)
{
	vcpu_set_intr(vcpu, HVIP_VSEIP);
}

static void vplic_vcpu_intr_deassert(struct acrn_vcpu *vcpu)
{
	vcpu_clear_intr(vcpu, HVIP_VSEIP);
}

static void vplic_update_pending(struct acrn_vplic *vplic)
{
	struct plic_regs *regs = &vplic->regs;
	for (uint32_t i = 0U; i < vplic_num_fields(vplic); i++) {
		uint32_t pending = regs->pending[i];
		uint32_t pin_level = vplic->pin_level[i];

		if (!pin_level)
			continue;

		for (uint32_t j = 0U; j < 32U; j++) {
			bool assert = pin_level & (1U << j);

			if (assert) {
				regs->pending[i] = pending | (1U << j);
			}
		}
	}
}

static void vplic_update_context(struct acrn_vplic *vplic, uint32_t context_id)
{
	struct acrn_vcpu *vcpu;
	uint32_t deliverable_irq;

	if (check_context_s_mode(vplic, context_id)) {
		vcpu = vcpu_from_vhartid(vplic->vm, vplic->info.contexts[context_id].hartid);
		deliverable_irq = vplic_get_deliverable_irq(vplic, context_id);
		if (deliverable_irq != 0U) {
			if (!vplic->asserted[context_id]) {
				vplic_vcpu_intr_assert(vcpu);
				vplic->asserted[context_id] = true;
			}
		} else {
			if (vplic->asserted[context_id]) {
				vplic_vcpu_intr_deassert(vcpu);
				vplic->asserted[context_id] = false;
			}
		}
	}
}

static void vplic_update(struct acrn_vplic *vplic)
{
        for (uint32_t context_id = 0U; context_id < vplic->info.context_num; context_id++) {
		vplic_update_context(vplic, context_id);
	}
}

static void vplic_read(struct acrn_vplic *vplic, uint64_t offset, uint32_t *data)
{
	struct plic_regs *regs = &vplic->regs;
	uint64_t flags;

	*data = 0UL;

	spinlock_irqsave_obtain(&vplic->lock, &flags);
	if ((offset >= vplic->priority_base &&
			offset <= (vplic->priority_base + (vplic->info.source_num << 2U)))) {
		uint32_t src_index = (offset - vplic->priority_base) >> 2U;

		*data = regs->source_priority[src_index];
	} else if (offset >= vplic->pending_base &&
			offset <= (vplic->pending_base + (vplic->info.source_num >> 3U))) {
		uint32_t word_index = (offset - vplic->pending_base) >> 2U;

		*data = regs->pending[word_index];
	} else if (offset >= vplic->enable_base &&
			offset < (vplic->enable_base + vplic->info.context_num * PLIC_ENABLE_STRIDE)) {
		uint32_t context_index = (offset - vplic->enable_base) / PLIC_ENABLE_STRIDE;
		uint32_t word_index = (offset & (PLIC_ENABLE_STRIDE - 1U)) >> 2U;

		if (check_context_s_mode(vplic, context_index)) {
			if (word_index < vplic_num_fields(vplic)) {
				*data = regs->enable[context_index][word_index];
			} else {
				dev_dbg(DBG_LEVEL_VPLIC, "vplic read: invalid enable offset 0x%x\n", offset);
			}
		} else {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic read: invalid context index %u\n", context_index);
		}
	} else if (offset >= vplic->context_base &&
			offset < (vplic->context_base + vplic->info.context_num * PLIC_CONTEXT_STRIDE)) {
		uint32_t context_index = (offset - vplic->context_base) / PLIC_CONTEXT_STRIDE;
		uint32_t reg_id = (offset & (PLIC_CONTEXT_STRIDE - 1U));

		if (check_context_s_mode(vplic, context_index)) {
			if (reg_id == PLIC_THRESHOLD_BASE) {
				*data = regs->threshold[context_index];
			} else if (reg_id == PLIC_EOI_BASE) {
				uint32_t irq = 0;

				irq = vplic_get_deliverable_irq(vplic, context_index);
				if (irq) {
					vplic_clear_pending(regs, irq);
					vplic_set_claimed(regs, context_index, irq);
				}
				vplic_update_context(vplic, context_index);
				*data = irq;
			} else {
				dev_dbg(DBG_LEVEL_VPLIC, "vplic read: invalid context reg id %u\n", reg_id);
			}
		} else {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic read: invalid context index %u\n", context_index);
		}
	} else {
		dev_dbg(DBG_LEVEL_VPLIC, "vplic read: invalid offset 0x%lx\n", offset);
	}
	spinlock_irqrestore_release(&vplic->lock, flags);

	dev_dbg(DBG_LEVEL_VPLIC, "%s: vmid:%d offset 0x%lx, data 0x%lx\n", __func__, vplic->vm->vm_id, *data);
	vplic_dump_regs(vplic);
}

static void vplic_write(struct acrn_vplic *vplic, uint64_t offset, uint32_t data)
{
	struct plic_regs *regs = &vplic->regs;
	uint64_t flags;

	dev_dbg(DBG_LEVEL_VPLIC, "%s: vmid:%d offset 0x%lx, data 0x%x", __func__, vplic->vm->vm_id, offset, data);
	spinlock_irqsave_obtain(&vplic->lock, &flags);
	if ((offset >= vplic->priority_base &&
			offset <= (vplic->priority_base + (vplic->info.source_num << 2U)))) {
		uint32_t src_index = (offset - vplic->priority_base) >> 2U;
		/* WARL register */
		if (data > vplic->info.max_priority)
			data = vplic->info.max_priority;
		regs->source_priority[src_index] = data;
                vplic_update(vplic);
	} else if (offset >= vplic->pending_base &&
		       offset <= (vplic->pending_base + (vplic->info.source_num >> 3U))) {
		dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid pending reg write 0x%lx\n", offset);
	} else if (offset >= vplic->enable_base &&
			offset < (vplic->enable_base + vplic->info.context_num * PLIC_ENABLE_STRIDE)) {
		uint32_t context_index = (offset - vplic->enable_base) / PLIC_ENABLE_STRIDE;
		uint32_t word_index = (offset & (PLIC_ENABLE_STRIDE - 1U)) >> 2U;

		if (check_context_s_mode(vplic, context_index)) {
			if (word_index < vplic_num_fields(vplic)) {
				regs->enable[context_index][word_index] = data;
				vplic_update_context(vplic, context_index);
			} else {
				dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid enable offset 0x%x\n", offset);
			}
		} else {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid context index %u\n", context_index);
		}
	} else if (offset >= vplic->context_base &&
			offset < (vplic->context_base + vplic->info.context_num * PLIC_CONTEXT_STRIDE)) {
		uint32_t context_index = (offset - vplic->context_base) / PLIC_CONTEXT_STRIDE;
		uint32_t reg_id = (offset & (PLIC_CONTEXT_STRIDE - 1U));

		if (check_context_s_mode(vplic, context_index)) {
			if (reg_id == PLIC_THRESHOLD_BASE) {
				/* WARL register */
				if (data > vplic->info.max_priority)
					data = vplic->info.max_priority;
				regs->threshold[context_index] = data;
				vplic_update_context(vplic, context_index);
			} else if (reg_id == PLIC_EOI_BASE) {
				if (data < vplic->info.source_num) {
					vplic_clear_claimed(regs, context_index, data);
					vplic_update_pending(vplic);
					vplic_update_context(vplic, context_index);
				}
			} else {
				dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid context reg id %u\n", reg_id);
			}
		} else {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid context index %u\n", context_index);
		}
	} else {
		dev_dbg(DBG_LEVEL_VPLIC, "vplic write: invalid offset 0x%x\n", offset);
	}
	spinlock_irqrestore_release(&vplic->lock, flags);

	vplic_dump_regs(vplic);
}

void vplic_accept_intr(struct acrn_vm *vm, uint32_t irq, bool assert)
{
	struct acrn_vplic *vplic;
	uint64_t flags;
	uint32_t pin_index = 0;
	uint32_t pin_mask = 0;

	vplic = &vm->arch_vm.vplic;
	if (!vplic->enabled)
		return;
	spinlock_irqsave_obtain(&vplic->lock, &flags);
	if (irq < vplic->info.source_num) {
		/* only set pending bit, clear operation is happening on claim read */
		pin_index = irq >> 5U;
		pin_mask = (1U << (irq & 31U));
		if (assert) {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic(vmid:%d) accept assert, irq:%u\n", vplic->vm->vm_id, irq);
			vplic->pin_level[pin_index] |= pin_mask;
			vplic_set_pending(&vplic->regs, irq);
			vplic_update(vplic);
		} else {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic(vmid:%d) accept deassert, irq:%u\n", vplic->vm->vm_id, irq);
			vplic->pin_level[pin_index] &= ~pin_mask;
		}
	} else {
		dev_dbg(DBG_LEVEL_VPLIC, "vplic(vmid:%d) ignoring irq %u", vplic->vm->vm_id, irq);
	}
	spinlock_irqrestore_release(&vplic->lock, flags);
}

int32_t vplic_access_handler(struct io_request *io_req, void *private_data)
{
	struct acrn_vplic *vplic = (struct acrn_vplic *)private_data;
	struct acrn_mmio_request *mmio = &io_req->reqs.mmio_request;
	uint64_t offset = mmio->address - vplic->info.base;
	int32_t ret = 0;

	if (mmio->size == 4U) {
		uint32_t data = (uint32_t)mmio->value;
		if (mmio->direction == ACRN_IOREQ_DIR_READ) {
			vplic_read(vplic, offset, &data);
			mmio->value = data;
		} else {
			vplic_write(vplic, offset, data);
		}
	} else {
		pr_err("All RW to PLIC must be 32-bits in size");
		ret = -EINVAL;
	}
	return ret;
}

void vplic_reset(struct acrn_vm *vm)
{
	struct acrn_vplic *vplic = &vm->arch_vm.vplic;
        struct plic_regs *regs;

        regs = &(vplic->regs);
        memset((void *)regs, 0U, sizeof(struct plic_regs));
}

static int fdt_find_plic_node(void *fdt) {
	int node = -1;

	node = fdt_node_offset_by_compatible(fdt, -1, "sifive,plic-1.0.0");
	if (node < 0) {
		node = fdt_node_offset_by_compatible(fdt, -1, "riscv,plic0");
	}
	return node;
}

int fdt_parse_plic_info(void *fdt, struct plic_info *plic)
{
	int plic_node, node, cpu_node, len;
	uint32_t num_entries, index;
	const fdt32_t *prop = NULL;
	uint32_t context_num = 0;
	int address_cells = 2;
	int size_cells = 2;
	int ret = 0;

	plic_node = fdt_find_plic_node(fdt);
	if (plic_node >= 0) {
		/* get max priority */
		prop = fdt_getprop(fdt, plic_node, "riscv,max-priority", &len);
		if (prop != NULL) {
			plic->max_priority = fdt32_to_cpu(*prop);
		} else {
			plic->max_priority = 7; /* default max priority */
		}
		/* get number of sources */
		prop = fdt_getprop(fdt, plic_node, "riscv,ndev", &len);
		if (prop != NULL) {
			plic->source_num = fdt32_to_cpu(*prop);
		} else {
			ret = -EINVAL;
		}
		/* get context info */
		prop = fdt_getprop(fdt, plic_node, "interrupts-extended", &len);
		if (prop != NULL && len > 0) {
			num_entries = len / (2 * sizeof(fdt32_t));
			if (num_entries > PLIC_VM_MAX_CONTEXTS) {
				num_entries = PLIC_VM_MAX_CONTEXTS;
			}
			for (index = 0; index < num_entries; index++) {
				node = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(prop[2 * index]));
				if (node >= 0) {
					uint32_t hwirq = fdt32_to_cpu(prop[2 * index + 1]);
					uint32_t hart_id = 0;
					cpu_node = fdt_parent_offset(fdt, node);
					if (cpu_node >= 0) {
						const fdt32_t *reg_prop = fdt_getprop(fdt, cpu_node, "reg", &len);
						if (reg_prop != NULL) {
							hart_id = fdt32_to_cpu(*reg_prop);
							plic->contexts[context_num].hartid = hart_id;
							plic->contexts[context_num].parent_hwirq = hwirq;
							context_num++;
						}
					}
				}
			}
		}
		if (context_num == 0) {
			ret = -EINVAL;
		}
		plic->context_num = context_num;

		/* get plic base address and size */
		prop = fdt_getprop(fdt, plic_node, "reg", &len);
		int node = fdt_parent_offset(fdt, plic_node);
		if (node >= 0) {
			const fdt32_t *cells_prop;
			cells_prop = fdt_getprop(fdt, node, "#address-cells", NULL);
			if (cells_prop) {
				address_cells = fdt32_to_cpu(*cells_prop);
			}
			cells_prop = fdt_getprop(fdt, node, "#size-cells", NULL);
			if (cells_prop) {
				size_cells = fdt32_to_cpu(*cells_prop);
			}
		}
		if (address_cells == 2 && size_cells == 2) {
			if (len == 16) {
				const fdt64_t *prop64 = (const fdt64_t *)prop;
				plic->base = fdt64_to_cpu(prop64[0]);
				plic->size = fdt64_to_cpu(prop64[1]);
			} else {
				ret = -EINVAL;
			}
		} else if (address_cells == 1 && size_cells == 1) {
			if (len == 8) {
				plic->base = fdt32_to_cpu(prop[0]);
				plic->size = fdt32_to_cpu(prop[1]);
			} else {
				ret = -EINVAL;
			}
		} else {
			ret = -EINVAL;
		}
	}
	return ret;
}

static void init_plic_info(struct acrn_vm *vm, struct plic_info *info)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	info->base = VPLIC_BASE;
	info->size = VPLIC_SIZE;
	info->source_num = VPLIC_MAX_NUM_SOURCE;
	info->max_priority = VPLIC_MAX_PRIORITY;
	/* context number is equal to VM CPU number */
	info->context_num = bitmap_weight(vm_config->cpu_affinity);

	for (uint32_t i = 0; i < info->context_num; i++) {
		info->contexts[i].hartid = i;
		info->contexts[i].parent_hwirq = IRQ_S_MODE;
	}
}

void vplic_init(struct acrn_vm *vm)
{
	struct acrn_vplic *vplic = &vm->arch_vm.vplic;

	spinlock_init(&vplic->lock);
	vplic->vm = vm;
	vplic->priority_base = PLIC_SRC_PRIO_BASE;
	vplic->pending_base = PLIC_PENDING_BASE;
	vplic->enable_base = PLIC_ENABLE_BASE;
	vplic->context_base = PLIC_CONTEXT_BASE;
	init_plic_info(vm, &vplic->info);
	for (uint32_t i = 0; i < vplic->info.context_num; i++) {
		vplic->asserted[i] = false;
	}

	register_mmio_emulation_handler(vm, vplic_access_handler, vplic->info.base,
		vplic->info.base + vplic->info.size, (void *)vplic, false);

	stg2pt_del_mr(vm, vm->root_stg2ptp, vplic->info.base, vplic->info.size);

	memset(&vplic->regs, 0U, sizeof(struct plic_regs));
	vplic->enabled = true;
}
