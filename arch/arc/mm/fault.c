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
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) ARC International
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/console.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>

#include <asm/hardirq.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/traps.h>

extern void die(const char *,struct pt_regs *,long,long);
extern int fixup_exception(struct pt_regs *regs);

static inline int notify_page_fault(struct pt_regs *regs, unsigned long cause)
{
    int ret = 0;

#ifdef CONFIG_KPROBES
    if (!user_mode(regs)) {
        preempt_disable();
        if (kprobe_running() && kprobe_fault_handler(regs, cause))
            ret = 1;
        preempt_enable();
    }
#endif

    return ret;
}

asmlinkage int do_page_fault(struct pt_regs *regs, int write,
                  unsigned long address, unsigned long cause)
{
    struct vm_area_struct *vma = NULL;
    struct task_struct *tsk = current;
    struct mm_struct *mm = tsk->mm;
    siginfo_t info;
    int fault;

    /*
     * We fault-in kernel-space virtual memory on-demand. The
     * 'reference' page table is init_mm.pgd.
     *
     * NOTE! We MUST NOT take any locks for this case. We may
     * be in an interrupt or a critical region, and should
     * only copy the information from the master page table,
     * nothing more.
     */
    if (address >= VMALLOC_START)
        goto vmalloc_fault;

    info.si_code = SEGV_MAPERR;
    /*
     * If we're in an interrupt or have no user
     * context, we must not take the fault..
     */
#ifndef CONFIG_ARC_FAULTS_DBG
    if (in_interrupt() || !mm)
        goto no_context;
#else
    if (in_interrupt()) {
        printk("Page fault in interrupt\n");
        goto no_context;
    }

    if (!mm) {
        printk("Page fault for task without a mm\n");
        goto no_context;
    }
#endif
    down_read(&mm->mmap_sem);
    vma = find_vma(mm, address);
    if (!vma)
        goto bad_area;
    if (vma->vm_start <= address)
        goto good_area;
    if (!(vma->vm_flags & VM_GROWSDOWN))
        goto bad_area;
    if (expand_stack(vma, address))
        goto bad_area;

	// vineetg-TODO:
	//shady_area:
	// Check if we ever land HERE, when address doesn't belong to vma,
	// but it's not BAD either
	// when we just fall thru into good_area

    /*
     * Ok, we have a good vm_area for this memory access, so
     * we can handle it..
     */
good_area:
    info.si_code = SEGV_ACCERR;

// Handle protection violation, execute on heap or stack

    if (cause == ((PROTECTION_VIOL <<16) | INST_FETCH_PROT_VIOL)) {
        goto bad_area;
    }

    if (write) {
        if (!(vma->vm_flags & VM_WRITE))
            goto bad_area;
    } else {
        if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
            goto bad_area;
    }

survive:
    /*
     * If for any reason at all we couldn't handle the fault,
     * make sure we exit gracefully rather than endlessly redo
     * the fault.
     */
    fault = handle_mm_fault(mm, vma, address, write);

    if (unlikely(fault & VM_FAULT_ERROR)) {
        if (fault & VM_FAULT_OOM)
            goto out_of_memory;
        else if (fault & VM_FAULT_SIGBUS)
            goto do_sigbus;
        BUG();
    }

    if ( fault & VM_FAULT_MAJOR)
        tsk->maj_flt++;
    else
        tsk->min_flt++;


    /* Diagnostic Code to force a signal such as Ctrl C to task */
#if 0
    if ( induce_problem && write && address == 0x14a00c) {
        info.si_signo = SIGINT;
        info.si_errno = 0;
        info.si_addr = (void *)address;
        force_sig_info(SIGINT, &info, tsk);
    }
#endif

	/* Fault Handled Gracefully, back to what user was trying to do */
    up_read(&mm->mmap_sem);
    return 0;

    /*
     * Something tried to access memory that isn't in our memory map..
     * Fix it, but check if it's kernel or user first..
     */
bad_area:
    up_read(&mm->mmap_sem);

      bad_area_nosemaphore:
    /* User mode accesses just cause a SIGSEGV */
    if (user_mode(regs)) {
        tsk->thread.fault_address = address;
        tsk->thread.cause_code = cause;
        info.si_signo = SIGSEGV;
        info.si_errno = 0;
        /* info.si_code has been set above */
        info.si_addr = (void *)address;
        force_sig_info(SIGSEGV, &info, tsk);
        return 1;
    }

no_context:
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
     * Oops. The kernel tried to access some bad page. We'll have to
     * terminate things with extreme prejudice.
     */

    die("Unable to handle kernel paging request", regs, address, cause);
    /* Game over.  */

    /*
     * We ran out of memory, or some other thing happened to us that made
     * us unable to handle the page fault gracefully.
     */
out_of_memory:
    if (tsk->pid == 1) {
        yield();
        goto survive;
    }
    up_read(&mm->mmap_sem);

    if (user_mode(regs))
        do_exit(SIGKILL);	/* This will never return */

    goto no_context;

do_sigbus:
    up_read(&mm->mmap_sem);

    /*
     * Send a sigbus, regardless of whether we were in kernel
     * or user mode.
     */
    tsk->thread.fault_address = address;
    tsk->thread.cause_code = cause;
    info.si_signo = SIGBUS;
    info.si_errno = 0;
    info.si_code = BUS_ADRERR;
    info.si_addr = (void *)address;
    force_sig_info(SIGBUS, &info, tsk);

    /* Kernel mode? Handle exceptions or die */
    if (!user_mode(regs))
        goto no_context;

    return 1;

vmalloc_fault:
    {
        /*
         * Synchronize this task's top level page-table
         * with the 'reference' page table.
         */
        int offset = pgd_index(address);
        pgd_t *pgd, *pgd_k;
        pmd_t *pmd, *pmd_k;

        pgd = tsk->active_mm->pgd + offset;
        pgd_k = init_mm.pgd + offset;

        if (!pgd_present(*pgd)) {
            if (!pgd_present(*pgd_k))
                goto bad_area_nosemaphore;
            set_pgd(pgd, *pgd_k);
            return 0;
        }

        pmd = pmd_offset(pgd, address);
        pmd_k = pmd_offset(pgd_k, address);

        if (pmd_present(*pmd) || !pmd_present(*pmd_k))
            goto bad_area_nosemaphore;
        set_pmd(pmd, *pmd_k);
    }

    return 0;
}
