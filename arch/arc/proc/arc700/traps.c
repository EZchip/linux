/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 *
 *
 *
 * Vineetg: June 10th 2008
 *   --Added show_callee_regs to display CALLEE REGS
 *   --Added show_fault_diagnostics as a common function to display all
 *     the REGS, trigger event logging etc
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 * Copyright (C)
 *
 * This program is free software you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author(s) : Rahul Trivedi
 * Exception handlers for the arc700
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/tlb.h>
#include <asm/arcregs.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/arcregs.h>
#include <asm/event-log.h>

extern int fixup_exception(struct pt_regs *regs);

/* vineetg: Feb 20th 2008
   If running on simulator in case of kernel crash, dont print the err
   msg as it adds abt 50k instructions to Instrn TRACE so the real cause
   of panic gets lost.
   Instead save the faulting instn and mem addr and do a hard stop
 */
unsigned int panic_cause_instn, panic_cause_mem, panic_cause_code;

/* "volatile" because it causes compiler to optimize away code.
 * Since running_on_hw is init to 1 at compile time, with -O2 compiler
 * throws away the code in die( ) which is a problem on ISS
 */
volatile int running_on_hw = 1;

DEFINE_SPINLOCK(die_lock);

void die(const char *str, struct pt_regs *regs, unsigned long address,
         unsigned long cause_reg)
{
    panic_cause_instn = regs->ret;
    panic_cause_mem  = address;
    panic_cause_code  = cause_reg;

    if (!running_on_hw) {
#ifdef CONFIG_SMP
        smp_send_stop();
#endif
        show_regs(regs);
        __asm__("flag 1");
    }

    show_kernel_fault_diag(str, regs, NULL, address, cause_reg);
    __asm__("flag 1");

    console_verbose();
    spin_lock_irq(&die_lock);
    bust_spinlocks(1);

    bust_spinlocks(0);
    spin_unlock_irq(&die_lock);
    do_exit(SIGSEGV);
}

static int do_fatal_exception(unsigned long cause, char *str, struct pt_regs *regs,
         siginfo_t * info)
{
    if (regs->status32 & STATUS_U_MASK) {
        struct task_struct *tsk = current;
#ifdef CONFIG_ARC_USER_FAULTS_DBG
        if (info->si_code != TRAP_BRKPT) {
            show_user_fault_diag(str, regs, NULL,
                                    (unsigned long)info->si_addr, cause);
        }
#endif              /* CONFIG_ARC_FAULTS_DBG */

        tsk->thread.fault_address = (unsigned int)info->si_addr;

		force_sig_info(info->si_signo, info, tsk);

    } else {
        /* Are we prepared to handle this kernel fault?
         *
         * (The kernel has valid exception-points in the source
         *  when it acesses user-memory. When it fails in one
         *  of those points, we find it in a table and do a jump
         *  to some fixup code that loads an appropriate error
         *  code)
         */
        if (fixup_exception(regs))
            return 0;

        /*
         * Oops. The kernel tried to access some bad page.
         * We'll have to terminate things with extreme prejudice.
         */
        die(str, regs, (unsigned long)info->si_addr, cause);
    }

    return 1;
}

#define DO_ERROR_INFO(signr, str, name, sicode) \
asmlinkage int name(unsigned long cause, unsigned long address, \
             struct pt_regs *regs) \
{ \
    siginfo_t info;\
    info.si_signo = signr;\
    info.si_errno = 0;\
    info.si_code = sicode;\
    info.si_addr = (void *)address;\
    return do_fatal_exception(cause,str,regs,&info);\
}

DO_ERROR_INFO(SIGSEGV, "Misaligned Access",
                do_misaligned_access, SEGV_ACCERR)
DO_ERROR_INFO(SIGILL, "Privileged Operation/Disabled Extension/Actionpoint Hit",
                do_privilege_fault, ILL_PRVOPC)
DO_ERROR_INFO(SIGILL, "Extenion Instruction Exception",
                do_extension_fault, ILL_ILLOPC)
DO_ERROR_INFO(SIGILL, "Illegal Instruction/Illegal Instruction Sequence",
                do_instruction_error, ILL_ILLOPC)
DO_ERROR_INFO(SIGBUS, "Access to Invalid Memory", do_memory_error, BUS_ADRERR)
DO_ERROR_INFO(SIGTRAP, "Breakpoint Set", do_trap_is_brkpt, TRAP_BRKPT)


asmlinkage void do_machine_check_fault(unsigned long cause, unsigned long address,
                  struct pt_regs *regs, struct callee_regs *cregs)
{
    show_kernel_fault_diag("Machine Check Exception", regs, cregs, address,
							cause);

    // DEAD END
    __asm__("flag 1");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
}

void __init trap_init(void)
{
    return;
}

asmlinkage void do_tlb_overlap_fault(unsigned long cause, unsigned long address,
                  struct pt_regs *regs)
{


// Modification by Simon Spooner of ARC
// The machine check exception could possibly be caused by duplicate
// entries getting into the JTLB due to a bug in the hardware that in
// very rare circumstances could allow duplicates.
// If the machine check was caused by a rogue PD in the TLB, delete it and carry on.
// Note that the MMU is switched off during this type of MachineCheck.
// If you don't switch it back on before exiting, then you go off into la la land.

    unsigned int tlbpd0[TLB_SIZE];
    unsigned int tlbpd1[TLB_SIZE];
    unsigned int n,z;

#ifdef CONFIG_ARC_TLB_DBG
    printk("EV_MachineCheck : Duplicate PD in TLB.  Attempting to correct\n");
#endif

    write_new_aux_reg(ARC_REG_PID, MMU_ENABLE | read_new_aux_reg(ARC_REG_PID));

    for(n=0;n<TLB_SIZE;n++)
    {
        write_new_aux_reg(ARC_REG_TLBINDEX,n);
        write_new_aux_reg(ARC_REG_TLBCOMMAND, TLBRead);
        tlbpd0[n] = read_new_aux_reg(ARC_REG_TLBPD0);
        tlbpd1[n] = read_new_aux_reg(ARC_REG_TLBPD1);
    }


   for(n=0;n<TLB_SIZE;n++)
   {
       for(z=0;z<TLB_SIZE;z++)
       {
           if(tlbpd0[z] & 0x400) // PD is valid.
           {
               if( (tlbpd0[z] == tlbpd0[n]) && (z!=n))  // Duplicate TLB
               {

#ifdef CONFIG_ARC_TLB_DBG
                   printk("Duplicate PD's @ %u and %u\n", n,z);
                   printk("TLBPD0[%u] : %08x TLBPD0[%u] : %08x\n",n,tlbpd0[n], z, tlbpd0[z]);
                   printk("Removing one of the duplicates\n");
#endif

                   tlbpd0[n]=0;
                   tlbpd1[n]=0;

                   write_new_aux_reg(ARC_REG_TLBPD0,0);
                   write_new_aux_reg(ARC_REG_TLBPD1,0);
                   write_new_aux_reg(ARC_REG_TLBINDEX,n);
                   write_new_aux_reg(ARC_REG_TLBCOMMAND,TLBWrite);

               }
           }
       }
    }

return;

}
