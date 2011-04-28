/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 *
 * Vineetg: March 25th Bug #92690
 *  -Major rewrite of Core ASID allocation routine get_new_mmu_context
 *
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
 *  linux/include/asm-arc/mmu_context.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_MMU_CONTEXT_H
#define _ASM_ARC_MMU_CONTEXT_H

#include <asm/arcregs.h>
#include <asm/tlb.h>

#include <asm-generic/mm_hooks.h>

#define FIRST_ASID  0
#define MAX_ASID    255 /* ARC 700 8 bit PID field in PID Aux reg */
#define NUM_ASID    ((MAX_ASID - FIRST_ASID) + 1 )
#define ASID_MASK   MAX_ASID
/* We use this to indicate that no ASID has been allocated to a mmu context */
#define NO_ASID     (MAX_ASID + 1)

/* ASID to mm struct mapping */
extern struct mm_struct *asid_mm_map[ NUM_ASID + 1 ];

extern volatile int asid_cache;

static inline void enter_lazy_tlb(struct mm_struct *mm,
                    struct task_struct *tsk)
{
}

/* Get a new mmu context or a hardware PID/ASID to work with.
 * If PID rolls over flush cache and tlb.
 */
static inline void
get_new_mmu_context(struct mm_struct *mm, int retiring_mm)
{
    int new_asid, mm_asid;
    struct mm_struct *prev_owner;
    unsigned long flags;

    /* TODO-vineet:
     *  1. SMP
     *  2. Can we reduce the duration of Interrupts lockout
     */
    local_irq_save(flags);

    mm_asid = mm->context.asid;

    /* Is this call from a flush_tlb_xxx () type function
     *  which wants to get rid of old address space
     */
    if ( retiring_mm ) {
        if (mm_asid == NO_ASID ) goto finish_up;
        //if ( current->active_mm->context.asid != mm_asid ) {
        if ( current->mm != mm ) {
            asid_mm_map[mm_asid] = (struct mm_struct *) NULL;
            mm->context.asid = NO_ASID;
            goto finish_up;
        }
    }

    /* If Task already has an ASID, it should give-up
     * After all he can have only one ASID.
     * As someone said, "to get something you have to forgoe something"
     */
    if ( mm_asid != NO_ASID )
        asid_mm_map[mm_asid] = (struct mm_struct *) NULL;

    new_asid = asid_cache;
    if ( ++new_asid > MAX_ASID) {
        /* Start a new asid allocation cycle */
        new_asid = FIRST_ASID;
        flush_icache_all();
        flush_tlb_all();
    }

    /* Are we sealing someone else's ASID. If yes, set that task's ASID to
     * invalid so when it runs it asks for a new ASID
     */
    if ( (prev_owner = asid_mm_map[new_asid]) ) {
        prev_owner->context.asid = NO_ASID;
    }

    /* Actual assignment of ASID to task */
    asid_mm_map[new_asid] = mm;
    mm->context.asid = new_asid;

    asid_cache = new_asid;

#ifdef  CONFIG_ARC_TLB_DBG
    printk ("ARC_TLB_DBG: NewMM=0x%x OldMM=0x%x task_struct=0x%x Task: %s,"
            " pid:%u, assigned asid:%lu\n",
            (unsigned int)mm, (unsigned int)prev_owner,
            (unsigned int)(mm->context.tsk), (mm->context.tsk)->comm,
            (mm->context.tsk)->pid, mm->context.asid);

    /* This is to double check TLB entries already exist for
     *    this newly allocated ASID
     */
    tlb_find_asid(new_asid);

#endif

    write_new_aux_reg(ARC_REG_PID, (new_asid | MMU_ENABLE));

finish_up:
    local_irq_restore(flags);
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
    mm->context.asid = NO_ASID;
#ifdef CONFIG_ARC_TLB_DBG
    mm->context.tsk = tsk;
#endif  /* CONFIG_ARC_TLB_DBG */
    return 0;
}

/* Switch the mm context */
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
                             struct task_struct *tsk)
{

    if ( next->context.asid > asid_cache) {
        get_new_mmu_context(next, 0);
    }
    else {
        write_new_aux_reg(ARC_REG_PID, (next->context.asid & 0xff) | MMU_ENABLE);
    }

#ifndef CONFIG_SMP    // In smp we use this reg for interrupt 1 scratch

    /* We keep the current processes PGD base ptr in the SCRATCH_DATA0
     * auxilliary reg for quick access in exception handlers.
     */
    write_new_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);
#endif

}

static inline void destroy_context(struct mm_struct *mm)
{
    int asid = mm->context.asid;

    asid_mm_map[asid] = NULL;
    mm->context.asid = NO_ASID;

}

#define deactivate_mm(tsk,mm)   do { } while (0)

static inline void
activate_mm (struct mm_struct *prev, struct mm_struct *next)
{
  /* Unconditionally get a new ASID */
  get_new_mmu_context(next, 0);

#ifndef CONFIG_SMP    // In smp we use this reg for interrupt 1 scratch

  write_new_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);
#endif
}
#endif  /* __ASM_ARC_MMU_CONTEXT_H */
