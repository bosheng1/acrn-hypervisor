/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 *   Haibo1 Xu <haibo1.xu@intel.com>
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

static void vplic_set_claimed(struct plic_regs *regs, uint32_t irq)
{
        vplic_reg_set_bit(&regs->claimed[irq >> 5U], irq & 31U);
}

static void vplic_clear_claimed(struct plic_regs *regs, uint32_t irq)
{
        vplic_reg_clear_bit(&regs->claimed[irq >> 5U], irq & 31U);
}

static uint32_t vplic_get_deliverable_irq(struct acrn_vplic *vplic, uint32_t context_id)
{
	uint32_t max_irq = 0U;
	struct plic_regs *regs = &vplic->regs;
	uint32_t max_prio = regs->threshold[context_id];

	for (uint32_t i = 0U; i < vplic_num_fields(vplic); i++) {
		uint32_t pending_enabled_not_claimed = (regs->pending[i] & ~regs->claimed[i]) &
							regs->enable[context_id][i];

		if (!pending_enabled_not_claimed)
			continue;

		for (uint32_t j = 0U; j < 32U; j++) {
			uint32_t irq = (i << 5U) + j;
			uint32_t prio = regs->source_priority[irq];
			bool enabled = !!(pending_enabled_not_claimed & (1U << j));

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
					vplic_set_claimed(regs, irq);
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
					vplic_clear_claimed(regs, data);
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

	vplic = &vm->arch_vm.vplic;
	if (!vplic->enabled)
		return;
	spinlock_irqsave_obtain(&vplic->lock, &flags);
	if (irq < vplic->info.source_num) {
		/* only set pending bit, clear operation is happening on claim read */
		if (assert) {
			dev_dbg(DBG_LEVEL_VPLIC, "vplic(vmid:%d) accept intr, irq:%u\n", vplic->vm->vm_id, irq);
			vplic_set_pending(&vplic->regs, irq);
			vplic_update(vplic);
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

	register_mmio_emulation_handler(vm, vplic_access_handler, vplic->info.base,
		vplic->info.base + vplic->info.size, (void *)vplic, false);

	stg2pt_del_mr(vm, vm->root_stg2ptp, vplic->info.base, vplic->info.size);

	memset(&vplic->regs, 0U, sizeof(struct plic_regs));
        for (uint32_t i = 0; i < vplic->info.context_num; i++) {
                vplic->asserted[i] = false;
        }
	vplic->enabled = true;
}
