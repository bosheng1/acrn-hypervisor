/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Haicheng Li <haicheng.li@intel.com>
 */

#include <errno.h>
#include <common/vcpu.h>
#include <asm/guest/instr_emul.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vcpu_priv.h>

#define INS32_OPCODE_MASK	0x7F
#define INS32_OPRD_MASK		0xF80
#define INS32_OPRS2_MASK	0x1F00000
#define INS32_OPSIZE_MASK	0x7000

#define INS32_OPCODE_LD		0x3
#define INS32_OPCODE_ST		0x23
#define INS32_OPSIZE_BYTE	0x0
#define INS32_OPSIZE_HALF	0x1
#define INS32_OPSIZE_WORD	0x2
#define INS32_OPSIZE_DWORD	0x3
#define INS32_OPSIZE_UBYTE	0x4
#define INS32_OPSIZE_UHALF	0x5
#define INS32_OPSIZE_UWORD	0x7

#define BIT64_MASK		0xffffffffffffffff
#define BIT32_MASK		0xffffffff
#define BIT16_MASK		0xffff
#define BIT8_MASK		0xff

/* TODO: why only ins32 here? Shall we emulate ins64 too? */
int32_t emulate_ins32(struct acrn_vcpu *vcpu, uint32_t ins)
{
	struct acrn_mmio_request *mmio_req = &vcpu->req.reqs.mmio_request;
	uint32_t reg_idx, op;
	uint32_t xlen = (ins & 0x3) == 0x1 ? 2: 4;
	uint64_t mask;
	uint64_t pc = vcpu->arch.regs.epc;
	int32_t rc = 0;
	uint32_t size = (ins & INS32_OPSIZE_MASK) >> 12;

	ASSERT((ins != 0), "ins 0x%x is not valid!", ins);

	switch (size) {
	case INS32_OPSIZE_BYTE:
	case INS32_OPSIZE_UBYTE:
		mask = BIT8_MASK;
		break;
	case INS32_OPSIZE_HALF:
	case INS32_OPSIZE_UHALF:
		mask = BIT16_MASK;
		break;
	case INS32_OPSIZE_WORD:
	case INS32_OPSIZE_UWORD:
		mask = BIT32_MASK;
		break;
	case INS32_OPSIZE_DWORD:
	default:
		mask = BIT64_MASK;
		break;
	}

	/* Skip to next instruction */
	vcpu_set_epc(vcpu, pc + xlen);

	op = INS32_OPCODE_MASK & ins;
	/* TODO: what is cmd if op==1? Per spec, bit[0]==1 means transformed ins or custom ins but
	 * not a cmd.
	 *
	 * Same question about op==0x21.
	 */
	if (op == INS32_OPCODE_LD || op == 0x1) {
		/* TODO: For (gpa == INVALID_HPA) in mmio_inst_fault_handler, shall we execute emulate_io()
		 * first to get the value from device, then write to guest context? After this, we should map
		 * the gpa (hva) so that the page fault is solved.
		 */

		/* For LOAD normal flow, mmio_inst_fault_handler will execute emulate_io first.
		 * In emulate_io_complete, the emulate_instruction will be called so that this
		 * branch can be executed to save value got from device into GP register.
		 */
		reg_idx = (ins & INS32_OPRD_MASK) >> 7;
		vcpu_set_gpcsr(vcpu, reg_idx, (mmio_req->value) & mask);
	} else if (op == INS32_OPCODE_ST || op == 0x21) {
		reg_idx = (ins & INS32_OPRS2_MASK) >> 20;
		mmio_req->value = (vcpu_get_gpcsr(vcpu, reg_idx) & mask);
	} else {
		rc = -EFAULT;
	}

	return rc;
}

int32_t emulate_instruction(struct acrn_vcpu *vcpu)
{
	uint32_t ins, ret = -EINVAL;

	ins = vcpu->arch.hctx.htinst;

	switch (ins & 0x3) {
		/* TODO: we suppose the htinst should have valid value but not 0 */
		case 0x1:
		case 0x3:
			ret = emulate_ins32(vcpu, ins);
			break;
		default:
			break;
	}

	return ret;
}

static int32_t decode_ins32(uint32_t ins)
{
	int size = (ins & INS32_OPSIZE_MASK) >> 12;

	switch (size) {
	case INS32_OPSIZE_BYTE:
	case INS32_OPSIZE_UBYTE:
		size = 1;
		break;
	case INS32_OPSIZE_HALF:
	case INS32_OPSIZE_UHALF:
		size = 2;
		break;
	case INS32_OPSIZE_WORD:
	case INS32_OPSIZE_UWORD:
		size = 4;
		break;
	case INS32_OPSIZE_DWORD:
	default:
		size = 8;
		break;
	}

	return size;
}

int32_t decode_instruction(uint32_t ins)
{
	int32_t ret = -EINVAL;
	uint32_t xlen = 0;

	xlen = ins & 0x3;
	switch (xlen) {
		/* TODO: we suppose the htinst should have valid value but not 0 */
		case 0x1:
		case 0x3:
			ret = decode_ins32(ins);
			break;
		default:
			break;
	}

	return ret;
}
