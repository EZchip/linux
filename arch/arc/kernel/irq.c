/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2008
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
 * arch/arc/kernel/irq.c
 *
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

/* table for system interrupt handlers */

static struct irqaction *irq_list[NR_IRQS];
static int irq_depth[NR_IRQS];

/* IRQ status spinlock - enable, disable */
static spinlock_t irq_controller_lock;

extern void smp_ipi_init(void);

void __init arc_irq_init(void)
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

/* initialise the irq table */
void __init init_IRQ(void)
{
    int i;

    for (i = 0; i < NR_IRQS; i++) {
        irq_list[i] = NULL;
    }

#ifdef CONFIG_SMP
    smp_ipi_init();
#endif
}

/* setup_irq:
 * Typically used by architecure special interrupts for
 * registering handler to IRQ line
 */

int setup_irq(unsigned int irq, struct irqaction *node)
{
    unsigned long flags;
    struct irqaction **curr;

    printk("---IRQ Request (%d) ISR ", irq);
    __print_symbol("%s\n",(unsigned long) node->handler);

    spin_lock_irqsave(&irq_controller_lock, flags); /* insert atomically */

    /* IRQ might be shared, thus we need a link list per IRQ for all ISRs
     * Adds to tail of list
     */
    curr = &irq_list[irq];

    while (*curr) {
        curr = &((*curr)->next);
    }

    *curr = node;

    /* If this IRQ slot is enabled for first time (shared IRQ),
     * enable vector on CPU side
     */
    if (irq_list[irq] == node) {
        unmask_interrupt((1<<irq)); // AUX_IEMABLE
    }

    spin_unlock_irqrestore(&irq_controller_lock, flags);
    return 0;
}

/* request_irq:
 * Exported to device drivers / modules to assign handler to IRQ line
 */
int request_irq(unsigned int irq,
        irqreturn_t (*handler)(int, void *),
        unsigned long flags, const char *name, void *dev_id)
{
    struct irqaction *node;
    int retval;

    if (irq >= NR_IRQS) {
        printk("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
        return -ENXIO;
    }

    node = (struct irqaction *)kmalloc(sizeof(struct irqaction), GFP_KERNEL);
    if (!node)
        return -ENOMEM;

    node->handler = handler;
    node->flags = flags;
    node->dev_id = dev_id;
    node->name = name;
    node->next = NULL;

    /* insert the new irq registered into the irq list */

    retval = setup_irq(irq, node);
    if (retval)
        kfree(node);
    return retval;
}
EXPORT_SYMBOL(request_irq);

/* free an irq node for the irq list */

void free_irq(unsigned int irq, void *dev_id)
{
    unsigned long flags;
    struct irqaction *tmp = NULL, **node;

    if (irq >= NR_IRQS) {
        printk("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
        return;
    }

    spin_lock_irqsave(&irq_controller_lock, flags); /* delete atomically */

    node = &irq_list[irq];

    while (*node) {
        if ((*node)->dev_id == dev_id) {
            tmp = *node;
            (*node) = (*node)->next;
            kfree(tmp);
        }
        else {
            node = &((*node)->next);
        }
    }

    spin_unlock_irqrestore(&irq_controller_lock, flags);

    if (!tmp)
        printk("%s: tried to remove invalid interrupt", __FUNCTION__);

}
EXPORT_SYMBOL(free_irq);

/* handle the irq */
void process_interrupt(unsigned int irq, struct pt_regs *fp)
{
    struct pt_regs *old = set_irq_regs(fp);
    struct irqaction *node;

    irq_enter();

    /* call all the ISR's in the list for that interrupt source */
    node = irq_list[irq];
    while (node) {
        kstat_cpu(smp_processor_id()).irqs[irq]++;
        node->handler(irq, node->dev_id);
        node = node->next;
    }

#ifdef  ARC_IRQ_DBG
    if (!irq_list[irq])
        printk(KERN_ERR "Spurious interrupt : irq no %u on cpu %u", irq,
                    smp_processor_id());
#endif

    irq_exit();

    set_irq_regs(old);
    return;
}

/* IRQ Autodetect not required for ARC
 * However the stubs still need to be exported for IDE et all
 */
unsigned long probe_irq_on(void)
{
    return 0;
}
EXPORT_SYMBOL(probe_irq_on);

int probe_irq_off(unsigned long irqs)
{
    return 0;
}
EXPORT_SYMBOL(probe_irq_off);

/* FIXME: implement if necessary */
void init_irq_proc(void)
{
    // for implementing /proc/irq/xxx
}

int show_interrupts(struct seq_file *p, void *v)
{
   int i = *(loff_t *) v, j;

    if(i == 0)          // First line, first CPU
    {
        seq_printf(p,"\t");
        for_each_online_cpu(j)
            seq_printf(p,"CPU%-8d",j);
        seq_putc(p,'\n');
    }

    if (i < NR_IRQS)
    {
        if(irq_list[i] != NULL)
        {
            seq_printf(p,"%u:\t",i);
            if(strlen(irq_list[i]->name) < 8)
                for_each_online_cpu(j)
                    seq_printf(p,"%s\t\t\t%u\n", irq_list[i]->name,kstat_cpu(j).irqs[i]);

            else
                for_each_online_cpu(j)
                    seq_printf(p,"%s\t\t%u\n", irq_list[i]->name,kstat_cpu(j).irqs[i]);
        }
    }


    return 0;

}
/**
 *      disable_irq - disable an irq and wait for completion
 *      @irq: Interrupt to disable
 *
 *      Disable the selected interrupt line.  We do this lazily.
 *
 *      This function may be called from IRQ context.
 */
void disable_irq(unsigned int irq)
{
    unsigned long flags;

    if (irq < NR_IRQS && irq_list[irq]) {
        spin_lock_irqsave(&irq_controller_lock, flags);
        if (!irq_depth[irq]++) {
            mask_interrupt(1<<irq);
        }
        spin_unlock_irqrestore(&irq_controller_lock, flags);
    }
    else {
        // printk("Incorrect IRQ action %d %s\n",irq, __FUNCTION__);
    }
}

EXPORT_SYMBOL(disable_irq);
/**
 *      enable_irq - enable interrupt handling on an irq
 *      @irq: Interrupt to enable
 *
 *      Re-enables the processing of interrupts on this IRQ line.
 *      Note that this may call the interrupt handler, so you may
 *      get unexpected results if you hold IRQs disabled.
 *
 *      This function may be called from IRQ context.
 */
void enable_irq(unsigned int irq)
{
    unsigned long flags;

    if (irq < NR_IRQS && irq_list[irq]) {
        spin_lock_irqsave(&irq_controller_lock, flags);
        if (irq_depth[irq]) {
            if (!--irq_depth[irq]) {
                unmask_interrupt(1<<irq);
            }
        }
        else {
            printk("Unbalanced IRQ action %d %s\n", irq, __FUNCTION__);
        }
        spin_unlock_irqrestore(&irq_controller_lock, flags);
    }
    else {
        // printk("Incorrect IRQ action %d %s\n",irq, __FUNCTION__);
    }
}

EXPORT_SYMBOL(enable_irq);

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

void local_irq_enable(void) {

    unsigned long flags;
    local_save_flags(flags);

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

    local_irq_restore(flags);
}

#else  /* ! CONFIG_ARCH_ARC_LV2_INTR */

 /* Simpler version for only 1 level of interrupt
  * Here we only Worry about Level 1 Bits
  */

void local_irq_enable(void) {

    unsigned long flags;
    local_save_flags(flags);
    flags |= (STATUS_E1_MASK | STATUS_E2_MASK);

    /* If called from hard ISR (between irq_enter and irq_exit)
     * don't allow Level 1. In Soft ISR we allow further Level 1s
     */

    if ( in_irq() ) {
        flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
    }

    local_irq_restore(flags);
}
#endif

EXPORT_SYMBOL(local_irq_enable);
