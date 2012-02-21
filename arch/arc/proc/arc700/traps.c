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
#include <linux/kdebug.h>
#include <asm/event-log.h>

extern int fixup_exception(struct pt_regs *regs);
void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
                        unsigned long address, unsigned long cause_reg);


/* "volatile" because it causes compiler to optimize away code.
 * Since running_on_hw is init to 1 at compile time, with -O2 compiler
 * throws away the code in die( ) which is a problem on ISS
 */
volatile int running_on_hw = 1;

void die(const char *str, struct pt_regs *regs, unsigned long address,
         unsigned long cause_reg)
{
    if (running_on_hw) {
        show_kernel_fault_diag(str, regs, address, cause_reg);
    }

    // DEAD END
    __asm__("flag 1");
}

static int noinline do_fatal_exception(unsigned long cause, char *str,
        struct pt_regs *regs, siginfo_t * info)
{
    if (user_mode(regs)) {
        struct task_struct *tsk = current;

        tsk->thread.fault_address = (unsigned int)info->si_addr;
        tsk->thread.cause_code = cause;

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


void do_machine_check_fault( unsigned long cause, unsigned long address,
    struct pt_regs *regs)
{
    die("Machine Check Exception",regs, address, cause);
}

void __init trap_init(void)
{
    return;
}


asmlinkage void do_trap_is_kprobe(unsigned long cause, unsigned long address,
                                                            struct pt_regs *regs)
{
    notify_die(DIE_TRAP, "kprobe_trap", regs, address, cause, SIGTRAP);
}

asmlinkage void do_trap(unsigned long cause, unsigned long address,
                  struct pt_regs *regs)
{
    unsigned int param = cause & 0xff;

    switch(param)
    {
        case 1:
            do_trap_is_brkpt(cause, address, regs);
            break;

        case 2:
            do_trap_is_kprobe(param, address, regs);
            break;

        default:
            break;
    }
}

asmlinkage void do_insterror_or_kprobe(unsigned long cause,
    unsigned long address, struct pt_regs *regs)
{
    /* Check if this exception is caused by kprobes */
    if(notify_die(DIE_IERR, "kprobe_ierr", regs, address,
                    cause, SIGILL) == NOTIFY_STOP)
        return;

    do_instruction_error(cause, address, regs);
}
