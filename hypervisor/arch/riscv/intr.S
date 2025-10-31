/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/trap.h>
#include <cpu.h>

	.text

	.balign 4
	.globl strap_handler
strap_handler:
	/*
	 * Swap sscratch and t0
	 * sscratch == 0 before trap: trap comes from hs-mode
	 * sscratch != 0 before trap: trap comes from v-mode and sscratch points to
	 *  start address of vcpu->arch.cpu_regs
	 */
	csrrw t0, sscratch, t0
	bnez t0, _save_general_ctx

	/* Trap comes from hs-mode, store trap frame to stack */
	addi sp, sp, -CPU_REGS_OFFSET_LAST
	mv t0, sp

_save_general_ctx:
	/* General purpose registers. */
	sd sp, CPU_REGS_OFFSET_SP(t0)
	sd ra, CPU_REGS_OFFSET_RA(t0)
	sd gp, CPU_REGS_OFFSET_GP(t0)
	sd tp, CPU_REGS_OFFSET_TP(t0)
	sd t1, CPU_REGS_OFFSET_T1(t0)
	sd t2, CPU_REGS_OFFSET_T2(t0)
	sd s0, CPU_REGS_OFFSET_S0(t0)
	sd s1, CPU_REGS_OFFSET_S1(t0)
	sd a0, CPU_REGS_OFFSET_A0(t0)
	sd a1, CPU_REGS_OFFSET_A1(t0)
	sd a2, CPU_REGS_OFFSET_A2(t0)
	sd a3, CPU_REGS_OFFSET_A3(t0)
	sd a4, CPU_REGS_OFFSET_A4(t0)
	sd a5, CPU_REGS_OFFSET_A5(t0)
	sd a6, CPU_REGS_OFFSET_A6(t0)
	sd a7, CPU_REGS_OFFSET_A7(t0)
	sd s2, CPU_REGS_OFFSET_S2(t0)
	sd s3, CPU_REGS_OFFSET_S3(t0)
	sd s4, CPU_REGS_OFFSET_S4(t0)
	sd s5, CPU_REGS_OFFSET_S5(t0)
	sd s6, CPU_REGS_OFFSET_S6(t0)
	sd s7, CPU_REGS_OFFSET_S7(t0)
	sd s8, CPU_REGS_OFFSET_S8(t0)
	sd s9, CPU_REGS_OFFSET_S9(t0)
	sd s10, CPU_REGS_OFFSET_S10(t0)
	sd s11, CPU_REGS_OFFSET_S11(t0)
	sd t3, CPU_REGS_OFFSET_T3(t0)
	sd t4, CPU_REGS_OFFSET_T4(t0)
	sd t5, CPU_REGS_OFFSET_T5(t0)
	sd t6, CPU_REGS_OFFSET_T6(t0)

	/* Control and Status Registers (CSRs). */
	csrr t1, sstatus
	sd t1, CPU_REGS_OFFSET_STATUS(t0)
	csrr t2, scause
	sd t2, CPU_REGS_OFFSET_CAUSE(t0)
	csrr t3, stval
	sd t3, CPU_REGS_OFFSET_TVAL(t0)
	csrr t4, sscratch
	sd t4, CPU_REGS_OFFSET_T0(t0)
	csrr t5, hstatus
	sd t5, CPU_REGS_OFFSET_HSTATUS(t0)
	csrr t6, sepc
	sd t6, CPU_REGS_OFFSET_EPC(t0)

	/* clears sscratch */
	csrw sscratch, zero

	/* Skip loading host state if it's an h-mode trap */
	beq t0, sp, _call_trap_handler

	/* Load host state */
	ld sp, CPU_REGS_OFFSET_EXC_SP(t0)
	ld tp, CPU_REGS_OFFSET_HOST_TP(t0)
	ld gp, CPU_REGS_OFFSET_HOST_GP(t0)

_call_trap_handler:
	/** Pass sp in a0, as the first argument to dispatch_trap(). */
	mv	a0, sp
	/** Pass hstatus in a1, as the second argument to dispatch_trap() */
	mv 	a1, t5

	call	dispatch_trap

	.globl strap_return
strap_return:
	/*
	 * sscratch == 0: Resume to hs-mode, restore context from stack
	 * sscratch != 0: Resume to v-mode, restore context from sscratch, which
	 *   points to start address of vcpu->arch.cpu_regs
	 */
	csrr t0, sscratch
	bnez t0, _save_host_ctx

	/* restore from stack */
	mv t0, sp

	/* deallocate stack space of previous stack */
	ld sp, CPU_REGS_OFFSET_SP(t0)
	addi sp, sp, CPU_REGS_OFFSET_LAST
	sd sp, CPU_REGS_OFFSET_SP(t0)
	j _restore_general_ctx

_save_host_ctx:
	/* save host state */
	sd sp, CPU_REGS_OFFSET_EXC_SP(t0)
	sd tp, CPU_REGS_OFFSET_HOST_TP(t0)
	sd gp, CPU_REGS_OFFSET_HOST_GP(t0)

_restore_general_ctx:
	/* Control and Status Registers (CSRs). */
	ld t1, CPU_REGS_OFFSET_EPC(t0)
	csrw sepc, t1
	ld t2, CPU_REGS_OFFSET_STATUS(t0)
	csrw sstatus, t2
	ld t3, CPU_REGS_OFFSET_HSTATUS(t0)
	csrw hstatus, t3

	/* General purpose registers. */
	ld sp, CPU_REGS_OFFSET_SP(t0)
	ld ra, CPU_REGS_OFFSET_RA(t0)
	ld gp, CPU_REGS_OFFSET_GP(t0)
	ld tp, CPU_REGS_OFFSET_TP(t0)
	ld t1, CPU_REGS_OFFSET_T1(t0)
	ld t2, CPU_REGS_OFFSET_T2(t0)
	ld s0, CPU_REGS_OFFSET_S0(t0)
	ld s1, CPU_REGS_OFFSET_S1(t0)
	ld a0, CPU_REGS_OFFSET_A0(t0)
	ld a1, CPU_REGS_OFFSET_A1(t0)
	ld a2, CPU_REGS_OFFSET_A2(t0)
	ld a3, CPU_REGS_OFFSET_A3(t0)
	ld a4, CPU_REGS_OFFSET_A4(t0)
	ld a5, CPU_REGS_OFFSET_A5(t0)
	ld a6, CPU_REGS_OFFSET_A6(t0)
	ld a7, CPU_REGS_OFFSET_A7(t0)
	ld s2, CPU_REGS_OFFSET_S2(t0)
	ld s3, CPU_REGS_OFFSET_S3(t0)
	ld s4, CPU_REGS_OFFSET_S4(t0)
	ld s5, CPU_REGS_OFFSET_S5(t0)
	ld s6, CPU_REGS_OFFSET_S6(t0)
	ld s7, CPU_REGS_OFFSET_S7(t0)
	ld s8, CPU_REGS_OFFSET_S8(t0)
	ld s9, CPU_REGS_OFFSET_S9(t0)
	ld s10, CPU_REGS_OFFSET_S10(t0)
	ld s11, CPU_REGS_OFFSET_S11(t0)
	ld t3, CPU_REGS_OFFSET_T3(t0)
	ld t4, CPU_REGS_OFFSET_T4(t0)
	ld t5, CPU_REGS_OFFSET_T5(t0)
	ld t6, CPU_REGS_OFFSET_T6(t0)
	ld t0, CPU_REGS_OFFSET_T0(t0)

	sret
