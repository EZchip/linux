/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: May 2011
 *  -Refactored get_new_mmu_context( ) to only handle live-mm.
 *   retiring-mm handled in other hooks
 *
 * vineetg: April 2011
 *  -CONFIG_ARC_MMU_SASID: support for ARC MMU Shared Address spaces
 *     activate_mm() and switch_mm() to setup MMU SASID reg with task's
 *     current sasid subscriptions
 *
 * Vineetg: March 25th, 2008: Bug #92690
 *  -Major rewrite of Core ASID allocation routine get_new_mmu_context
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_MMU_CONTEXT_H
#define _ASM_ARC_MMU_CONTEXT_H

#include <asm/arcregs.h>
#include <asm/tlb.h>

#ifndef CONFIG_ARC_CMN_MMAP
#include <asm-generic/mm_hooks.h>
#else
extern void arch_dup_mmap(struct mm_struct *oldmm, struct mm_struct *mm);
extern void arch_exit_mmap(struct mm_struct *mm);
#endif

/* ARC MMU provides 8-bit ASID (0..255) to TAG TLB entries, allowing entries
 * with same vaddr (different tasks) to co-exit. This provides for
 * "Fast Context Switch" i.e. no TLB flush on ctxt-switch
 *
 * Linux assigns each task a unique ASID. A simple round-robin allocation
 * of H/w ASID is done using software tracker @asid_cache.
 * When it reaches max 255, the allocation cycle starts afresh by flushing
 * the entire TLB and wrapping ASID back to zero.
 *
 * For book-keeping, Linux uses a couple of data-structures:
 *  -mm_struct has an @asid field to keep a note of task's ASID (needed at the
 *   time of say switch_mm( )
 *  -An array of mm structs @asid_mm_map[] for asid->mm the reverse mapping,
 *  given an ASID, finding the mm struct associated.
 *
 * The round-robin allocation algorithm allows for ASID stealing.
 * If asid tracker is at "x-1", a new req will allocate "x", even if "x" was
 * already assigned to another (switched-out) task. Obviously the prev owner
 * is marked with an invalid ASID to make it request for a new ASID when it
 * gets scheduled next time. However its TLB entries (with ASID "x") could
 * exist, which must be cleared before the same ASID is used by the new owner.
 * Flushing them would be plausible but costly solution. Instead we force a
 * allocation policy quirk, which ensures that a stolen ASID won't have any
 * TLB entries associates, alleviating the need to flush.
 * The quirk essentially is not allowing ASID allocated in prev cycle
 * to be used past a roll-over in the next cycle.
 * When this happens (i.e. task ASID > asid tracker), task needs to refresh
 * its ASID, aligning it to current value of tracker. If the task doesn't get
 * scheduled past a roll-over, hence its ASID is not yet realigned with
 * tracker, such ASID is anyways safely reusable because it is
 * gauranteed that TLB entries with that ASID wont exist.
 */

#define asid_cache(cpu)		(cpuinfo_arc700[cpu].asid_cache)
#define asid_mm_map(cpu, asid)	(cpuinfo_arc700[cpu].asid_mm_map[asid])

/* Assign a new ASID to task. If the task already has an ASID, it is
 * relinquished.
 */
static inline void get_new_mmu_context(struct mm_struct *mm)
{
	unsigned int cpu = smp_processor_id();
	struct mm_struct *prev_owner;
	unsigned long flags;

	local_irq_save(flags);

	/* Relinquish the currently owned ASID (if any).
	 * Doing unconditionally saves a cmp-n-branch; for already unused
	 * ASID slot, the value was/remains NULL
	 */
	asid_mm_map(cpu, mm->context.asid[cpu]) = (struct mm_struct *)NULL;

	/* move to new ASID */
	if (++asid_cache(cpu) > MAX_ASID) {	/* ASID roll-over */
		asid_cache(cpu) = FIRST_ASID;
		local_flush_tlb_all();
	}

	/*
	 * Is next ASID already owned by some-one else (we are stealing it).
	 * If so, let the orig owner be aware of this, so when it runs, it
	 * asks for a brand new ASID. This would only happen for a long-lived
	 * task with ASID from prev allocation cycle (before ASID roll-over).
	 *
	 * This might look wrong - if we are re-using some other task's ASID,
	 * won't we use it's stale TLB entries too. Actually switch_mm( ) takes
	 * care of such a case: it ensures that task with ASID from prev alloc
	 * cycle, when scheduled will refresh it's ASID: see switch_mm( ) below
	 * The stealing scenario described here will only happen if that task
	 * didn't get a chance to refresh it's ASID - implying stale entries
	 * won't exist.
	 */
	prev_owner = asid_mm_map(cpu, asid_cache(cpu));
	if (prev_owner)
		prev_owner->context.asid[cpu] = NO_ASID;

	/* Assign new ASID to tsk */
	asid_mm_map(cpu, asid_cache(cpu)) = mm;
	mm->context.asid[cpu] = asid_cache(cpu);

#ifdef CONFIG_ARC_TLB_DBG
	pr_info("ARC_TLB_DBG: NewMM=0x%x OldMM=0x%x task_struct=0x%x Task: %s,"
	       " pid:%u, assigned asid:%lu\n",
	       (unsigned int)mm, (unsigned int)prev_owner,
	       (unsigned int)(mm->context.tsk), (mm->context.tsk)->comm,
	       (mm->context.tsk)->pid, mm->context.asid);
#endif

	write_aux_reg(ARC_REG_PID, asid_cache(cpu) | MMU_ENABLE);

	local_irq_restore(flags);
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int i;

	for (i=0; i<NR_CPUS; i++)
		mm->context.asid[i] = NO_ASID;
#ifdef CONFIG_ARC_TLB_DBG
	mm->context.tsk = tsk;
#endif
	return 0;
}

/* Prepare the MMU for task: setup PID reg with allocated ASID
    If task doesn't have an ASID (never alloc or stolen, get a new ASID)
*/
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();

#ifndef CONFIG_SMP
	/* PGD cached in MMU reg to avoid 3 mem lookups: task->mm->pgd */
	write_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);
#endif

	/*
	 * Get a new ASID if task doesn't have a valid one. Possible when
	 *  -task never had an ASID (fresh after fork)
	 *  -it's ASID was stolen - past an ASID roll-over.
	 *  -There's a third obscure scenario (if this task is running for the
	 *   first time afer an ASID rollover), where despite having a valid
	 *   ASID, we force a get for new ASID - see comments at top.
	 *
	 * Both the non-alloc scenario and first-use-after-rollover can be
	 * detected using the single condition below:  NO_ASID = 256
	 * while asid_cache is always a valid ASID value (0-255).
	 */
	if (next->context.asid[cpu] > asid_cache(cpu)) {
		get_new_mmu_context(next);
	} else {
		/*
		 * XXX: This will never happen given the chks above
		 * BUG_ON(next->context.asid > MAX_ASID);
		 */
		write_aux_reg(ARC_REG_PID, next->context.asid[cpu] | MMU_ENABLE);
	}

	/* Clear cpu from the old mm, and set it in the new one. */
	cpumask_clear_cpu(cpu, mm_cpumask(prev));
	cpumask_set_cpu(cpu, mm_cpumask(next));


#ifdef CONFIG_ARC_MMU_SASID
	{
		unsigned int tsk_sasids;

		tsk_sasids = is_any_mmapcode_task_subscribed(next);
		if (tsk_sasids)
			write_aux_reg(ARC_REG_SASID, tsk_sasids);
	}
#endif

}

static inline void destroy_context(struct mm_struct *mm)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;

	local_irq_save(flags);

	cpumask_clear_cpu(cpu, mm_cpumask(mm));
	asid_mm_map(cpu, mm->context.asid[cpu]) = NULL;
	mm->context.asid[cpu] = NO_ASID;

	local_irq_restore(flags);
}

/* it seemed that deactivate_mm( ) is a reasonable place to do book-keeping
 * for retiring-mm. However destroy_context( ) still needs to do that because
 * between mm_release( ) = >deactive_mm( ) and
 * mmput => .. => __mmdrop( ) => destroy_context( )
 * there is a good chance that task gets sched-out/in, making it's ASID valid
 * again (this teased me for a whole day).
 */
#define deactivate_mm(tsk, mm)   do { } while (0)

static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned int cpu = smp_processor_id();

#ifndef CONFIG_SMP
	write_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);
#endif

	/* Unconditionally get a new ASID */
	get_new_mmu_context(next);

	cpumask_set_cpu(cpu, mm_cpumask(next));

#ifdef CONFIG_ARC_MMU_SASID
	BUG_ON((is_any_mmapcode_task_subscribed(next)));
#endif
}

#endif /* __ASM_ARC_MMU_CONTEXT_H */
