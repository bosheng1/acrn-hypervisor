/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/atomic.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/instr_emul.h>
#include <asm/pgtable.h>
#include <asm/trap.h>
#include <asm/cpu.h>
#include <io_req.h>
#include <trace.h>
#include <logmsg.h>

/* FIXME: temporary solution. The PIO support in risc-v should modify this func. */
void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address, uint32_t nbytes)
{
	(void)vm;
	(void)port_address;
	(void)nbytes;
}

/* FIXME: temporary solution. The PIO support in risc-v should modify this func. */
void emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	(void)vcpu;
	(void)io_req;
}

int32_t mmio_inst_fault_handler(struct acrn_vcpu *vcpu)
{
	int ret;
	int32_t status = 0;
	struct io_request *io_req = &vcpu->req;
	struct acrn_mmio_request *mmio_req = &io_req->reqs.mmio_request;
	struct cpu_regs *r = &vcpu->arch.regs;
	uint64_t cause = r->cause;
	uint64_t gva = r->tval;
	uint32_t ins = vcpu->arch.hctx.htinst;
	uint64_t gpa = (vcpu->arch.hctx.htval << 2) | (gva & 0x3);

	/* Handle page fault from guest */
	io_req->io_type = ACRN_IOREQ_TYPE_MMIO;

	/* Specify if read or write operation */
	switch (cause) {
	case TRAP_CAUSE_EXC_STORE_GUEST_PAGE_FAULT:
		/* Write operation */
		mmio_req->direction = ACRN_IOREQ_DIR_WRITE;
		mmio_req->value = 0UL;
		break;
	case TRAP_CAUSE_EXC_LOAD_GUEST_PAGE_FAULT:
		/* Read operation */
		mmio_req->direction = ACRN_IOREQ_DIR_READ;
		break;
	default:
		status = -1;
		pr_acrnlog("unsupported access to address: 0x%016lx", gpa);
		break;
	}

	mmio_req->address = gpa;
	ret = decode_instruction(ins);
	if ((status == 0) && (ret > 0)) {
		mmio_req->size = (uint64_t)ret;
		if (gpa == INVALID_HPA) {
			mmio_req->value = 0UL;
			emulate_instruction(vcpu);
		} else {
			/*
			 * For MMIO write, ask DM to run MMIO emulation after
			 * instruction emulation. For MMIO read, ask DM to run MMIO
			 * emulation at first.
			 */

			/* Determine value being written. */
			if (mmio_req->direction == ACRN_IOREQ_DIR_WRITE) {
				status = emulate_instruction(vcpu);
				if (status != 0) {
					ret = -EFAULT;
				}
			}

			if (ret > 0) {
				status = emulate_io(vcpu, io_req);
			}
		}
	}

	return status;
}
