/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Yifan Liu <yifan1.liu@intel.com>
 *         Haicheng Li <haicheng.li@intel.com>
 */

#include <vcpu.h>
#include <vm.h>
#include <cpu.h>
#include <errno.h>
#include <types.h>
#include <schedule.h>

#include <asm/trap.h>
#include <asm/guest/vcpu_priv.h>
#include <asm/guest/vsbi.h>
#include <asm/guest/vio.h>

int32_t vcpu_virtual_inst_fault_handler(struct acrn_vcpu *vcpu) {
	/* TODO: to be implemented */
	pr_err("Unhandled virtual instruction fault");
	(void)vcpu;
	cpu_dead();
	return 0;
}

int32_t vcpu_exit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t cause = vcpu->arch.regs.cause;
	int32_t ret;

	if ((cause & TRAP_CAUSE_INTERRUPT_BITMASK) != 0) {
		/*
		 * RISC-V traps does not acknowledge the interrupt (interrupt
		 * is still pending in sip). Call interrupt handler to clear
		 * the pending bit.
		 */

		/* const input: vcpu state does not change */
		dispatch_interrupt((const struct intr_excp_ctx *)&(vcpu->arch.regs));

		ret = 0;
		pr_dbg("vcpu intr, cause: 0x%x, ret: %d", cause, ret);

		/* Check if we have any other interrupts pending */
		local_irq_enable();
	} else {
		local_irq_enable();
		switch (cause) {
			case TRAP_CAUSE_EXC_VIRTUAL_SUPERVISOR_ECALL:
				ret = vsbi_exit_handler(vcpu);
				break;
			case TRAP_CAUSE_EXC_VIRTUAL_INST_FAULT:
				ret = vcpu_virtual_inst_fault_handler(vcpu);
				break;
			case TRAP_CAUSE_EXC_STORE_GUEST_PAGE_FAULT:
			case TRAP_CAUSE_EXC_LOAD_GUEST_PAGE_FAULT:
				ret = mmio_inst_fault_handler(vcpu);
				break;
			default:
				ret = -EINVAL;
				pr_err("Unhandled trap. cause: 0x%x", cause);
				break;
		}

		pr_dbg("vcpu exc, cause: 0x%x, ret: %d", cause, ret);
	}

	return ret;
}

void dispatch_vcpu_trap(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct acrn_vcpu *vcpu = get_running_vcpu(pcpu_id);
	struct cpu_regs *regs = &(vcpu->arch.regs);
	int32_t ret = -EINVAL;

	/* unload: save physical CSRs onto vcpu struct */
	unload_vcpu(vcpu);

	ret = vcpu_exit_handler(vcpu);
	if (ret < 0) {
		pr_err("Failed to handle vcpu exit. ret: %d", ret);
	}
	local_irq_disable();

	if (need_reschedule(pcpu_id)) {
		schedule();
	}

	ret = riscv_process_vcpu_requests(vcpu);
	if (ret < 0) {
		pr_fatal("Failed to process vcpu requests, pausing the vcpu");
		get_vm_lock(vcpu->vm);
		zombie_vcpu(vcpu);
		put_vm_lock(vcpu->vm);
		schedule();
		/* we'll never be scheduled in */
	}

	/*
	 * if handler marks vcpu context dirty by
	 * vcpu_mark_context_dirty, flush the context
	 * to physical CSRs
	 */
	flush_vcpu_context(vcpu);

	/* next trap frame saved directly to vcpu struct */
	cpu_csr_write(CSR_SSCRATCH, (uint64_t)regs);
}
