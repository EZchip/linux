/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Mar 2009
 *  -use generic irqaction to store IRQ requests
 *  -device ISRs no longer take pt_regs (rest of the kernel convention)
 *  -request_irq( ) definition matches declaration in inc/linux/interrupt.h
 *
 * Vineetg: Mar 2009 (Supporting 2 levels of Interrupts)
 *  -local_irq_enable shd be cognizant of current IRQ state
 *    It is lot more involved now and thus re-written in "C"
 *  -set_irq_regs in common ISR now always done and not dependent
 *      on CONFIG_PROFILEas it is used by
 *
 * Vineetg: Jan 2009
 *  -Cosmetic change to display the registered ISR name for an IRQ
 *  -free_irq( ) cleaned up to not have special first-node/other node cases
 *
 * Vineetg: June 17th 2008
 *  -Added set_irq_regs() to top level ISR for profiling
 *  -Don't Need __cli just before irq_exit(). Intr already disabled
 *  -Disabled compile time ARC_IRQ_DBG
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <asm/errno.h>
#include <linux/kallsyms.h>
#include <linux/irqflags.h>
#include <linux/kernel_stat.h>
#include <linux/slab.h>

//#define ARC_IRQ_DBG

extern void smp_ipi_init(void);


static void arc_mask_interrupt(struct irq_data *data)
{
	mask_interrupt(1 << data->irq);
}

static void arc_unmask_interrupt(struct irq_data *data)
{
	unmask_interrupt(1 << data->irq);
}

static struct irq_chip arc_chip = {
	.name			= "ARC",
	.irq_ack		= arc_mask_interrupt,
	.irq_mask		= arc_mask_interrupt,
	.irq_unmask 	= arc_unmask_interrupt,
	.irq_disable	= arc_mask_interrupt,
};

static void __init arc_irq_init(void)
{
    extern int _int_vec_base_lds;

    /* set the base for the interrupt vector tabe as defined in Linker File
       Interrupts will be enabled in start_kernel
     */
    write_new_aux_reg(AUX_INTR_VEC_BASE, &_int_vec_base_lds);

    /* vineetg: Jan 28th 2008
       Disable all IRQs on CPU side
       We will selectively enable them as devices request for IRQ
     */
    write_new_aux_reg(AUX_IENABLE, 0);

#ifdef CONFIG_ARCH_ARC_LV2_INTR
{
    int level_mask = 0;
    /* If any of the peripherals is Level2 Interrupt (high Prio),
       set it up that way
     */
#ifdef  CONFIG_TIMER_LV2
    level_mask |= (1 << TIMER0_INT );
#endif

#ifdef  CONFIG_SERIAL_LV2
    level_mask |= (1 << VUART_IRQ);
#endif

#ifdef  CONFIG_EMAC_LV2
    level_mask |= (1 << VMAC_IRQ );
#endif

    if (level_mask) {
        printk("setup as level-2 interrupt/s \n");
        write_new_aux_reg(AUX_IRQ_LEV, level_mask);
    }
}
#endif

}

void __init init_IRQ(void)
{
	int i;

	for (i = 0; i < NR_IRQS; i++) {
		irq_set_noprobe(i);
	}

	for (i = 0; i < NR_LEVEL_IRQ ;i++) {
		irq_set_chip_and_handler(i, &arc_chip, handle_level_irq);
	}

    arc_irq_init();

#ifdef CONFIG_SMP
	smp_ipi_init();
#endif
}

/* handle the irq */
void do_IRQ(unsigned int irq, struct pt_regs *fp)
{
    struct pt_regs *old = set_irq_regs(fp);

    irq_enter();

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (unlikely(irq >= NR_IRQS)) {
		if (printk_ratelimit())
			printk(KERN_WARNING "Bad IRQ%u\n", irq);
		ack_bad_irq(irq);
	} else {
		generic_handle_irq(irq);
	}

    irq_exit();

    set_irq_regs(old);
    return;
}

int arch_show_interrupts(struct seq_file *p, int prec)
{
	return 0;
}

#ifdef CONFIG_SMP

int get_hw_config_num_irq()
{
    uint32_t val = read_new_aux_reg(ARC_REG_VECBASE_BCR);

    switch(val & 0x03)
    {
        case 0: return 16;
        case 1: return 32;
        case 2: return 8;
        default: return 0;
    }

    return 0;
}

#endif

/* Enable interrupts.
 * 1. Explicitly called to re-enable interrupts
 * 2. Implicitly called from spin_unlock_irq, write_unlock_irq etc
 *    which maybe in hard ISR itself
 *
 * Semantics of this function change depending on where it is called from:
 *
 * -If called from hard-ISR, it must not invert interrupt priorities
 *  e.g. suppose TIMER is high priority (Level 2) IRQ
 *    Time hard-ISR, timer_interrupt( ) calls spin_unlock_irq several times.
 *    Here local_irq_enable( ) shd not re-enable lower priority interrupts
 * -If called from soft-ISR, it must re-enable all interrupts
 *    soft ISR are low prioity jobs which can be very slow, thus all IRQs
 *    must be enabled while they run.
 *    Now hardware context wise we may still be in L2 ISR (not done rtie)
 *    still we must re-enable both L1 and L2 IRQs
 *  Another twist is prev scenario with flow being
 *     L1 ISR ==> interrupted by L2 ISR  ==> L2 soft ISR
 *     here we must not re-enable Ll as prev Ll Interrupt's h/w context will get
 *     over-written (this is deficiency in ARC700 Interrupt mechanism)
 */

#ifdef CONFIG_ARCH_ARC_LV2_INTR     // Complex version for 2 levels of Intr

void arch_local_irq_enable(void) {

    unsigned long flags;
    flags = arch_local_save_flags();

    /* Allow both L1 and L2 at the onset */
    flags |= (STATUS_E1_MASK | STATUS_E2_MASK);

    /* Called from hard ISR (between irq_enter and irq_exit) */
    if ( in_irq() ) {

        /* If in L2 ISR, don't re-enable any further IRQs as this can cause
         * IRQ priorities to get upside down.
         * L1 can be taken while in L2 hard ISR which is wron in theory ans
         * can also cause the dreaded L1-L2-L1 scenario
         */
        if ( flags & STATUS_A2_MASK ) {
            flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
        }

        /* Even if in L1 ISR, allowe Higher prio L2 IRQs */
        else if ( flags & STATUS_A1_MASK ) {
            flags &= ~(STATUS_E1_MASK);
        }
    }

    /* called from soft IRQ, ideally we want to re-enable all levels */

    else if ( in_softirq() ) {

        /* However if this is case of L1 interrupted by L2,
         * re-enabling both may cause whaco L1-L2-L1 scenario
         * because ARC700 allows level 1 to interrupt an active L2 ISR
         * Thus we disable both
         * However some code, executing in soft ISR wants some IRQs to be
         * enabled so we re-enable L2 only
         *
         * How do we determine L1 intr by L2
         *  -A2 is set (means in L2 ISR)
         *  -E1 is set in this ISR's pt_regs->status32 which is
         *      saved copy of status32_l2 when l2 ISR happened
         */
        struct pt_regs *pt = get_irq_regs();
        if ( (flags & STATUS_A2_MASK ) && pt &&
              (pt->status32 & STATUS_A1_MASK ) ) {
            //flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
            flags &= ~(STATUS_E1_MASK);
        }
    }

    arch_local_irq_restore(flags);
}

#else  /* ! CONFIG_ARCH_ARC_LV2_INTR */

 /* Simpler version for only 1 level of interrupt
  * Here we only Worry about Level 1 Bits
  */

void arch_local_irq_enable(void) {

    unsigned long flags;
    flags = arch_local_save_flags();
    flags |= (STATUS_E1_MASK | STATUS_E2_MASK);

    /* If called from hard ISR (between irq_enter and irq_exit)
     * don't allow Level 1. In Soft ISR we allow further Level 1s
     */

    if ( in_irq() ) {
        flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
    }

    arch_local_irq_restore(flags);
}
#endif

EXPORT_SYMBOL(arch_local_irq_enable);
