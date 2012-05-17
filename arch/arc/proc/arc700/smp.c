/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * RajeshwarR: Dec 11, 2007
 *   -- Added support for Inter Processor Interrupts
 *
 * Vineetg: Nov 1st, 2007
 *    -- Initial Write (Borrowed heavily from ARM)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/irq.h>

#include <asm/atomic.h>
#include <asm/processor.h>
#include <asm/percpu.h>
#ifdef CONFIG_ARCH_ARC800
#include <asm/idu.h>
#endif
#include <asm/mmu_context.h>
#include <linux/delay.h>

extern int _int_vec_base_lds;
extern struct task_struct *_current_task[NR_CPUS];

extern void wakeup_secondary(int);
extern void first_lines_of_secondary(void);
extern void arc_clockevent_init(void);
extern void setup_processor(void);
extern void arc_irq_init(void);
secondary_boot_t secondary_boot_data;

/* Called from start_kernel */
void __init smp_prepare_boot_cpu(void)
{
    void **c_entry = (void **)CPU_SEC_ENTRY_POINT;

    *c_entry = first_lines_of_secondary;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
    unsigned int i;

    for (i = 0; i < NR_CPUS; i++)
        cpu_set(i, cpu_possible_map);
}

/* called from init ( ) =>  process 1 */
void __init smp_prepare_cpus(unsigned int max_cpus)
{
    int i;
    unsigned int cpu = smp_processor_id();

    cpuinfo_arc700[cpu].lpj = loops_per_jiffy;

    /*
     * Initialise the present map, which describes the set of CPUs
     * actually populated at the present time.
     */
    for (i = 0; i < max_cpus; i++)
        cpu_set(i, cpu_present_map);
}

void __init smp_cpus_done(unsigned int max_cpus)
{

}

asmlinkage void __cpuinit start_kernel_secondary(void)
{
    struct mm_struct *mm = &init_mm;
    unsigned int cpu = smp_processor_id();

    /* Do CPU init
        1. Detect CPU Type and its config
        2. TLB Init
        3. Setup Vector Tbl Base Address
        4. Maskoff all IRQs
        IMP!!! Dont do any fancy stuff inside this call as we have
        not yet setup mm contexts etc yet
     */
    setup_processor();

    _current_task[cpu] = current;

    atomic_inc(&mm->mm_users);
    atomic_inc(&mm->mm_count);
    current->active_mm = mm;
	cpumask_set_cpu(cpu, mm_cpumask(mm));

    // TODO-vineetg: need to implement this call
    //enter_lazy_tlb(mm, current);

    /* vineetg Nov 19th 2007:
        For this printk to work in ISS, a bridge.dll instance in the
        uart address space is needed so that uart access by non-owner
        CPU are shunted to the other CPU which owns the UART.
     */

    printk(KERN_INFO "## CPU%u LIVE ##: Executing Code...\n", cpu);

    arc_irq_init();

	calibrate_delay();
    cpuinfo_arc700[cpu].lpj = loops_per_jiffy;

    arc_clockevent_init();

    /* Enable the interrupts on the local cpu */
    local_irq_enable();

    /* Note that preemption needs to be disabled before entering
       cpu_idle. When it sees need_resched it enables it momentarily
       But most of then time, idle is running, preemption is disabled
     */
    preempt_disable();
    cpu_set(cpu, cpu_online_map);
    cpu_idle();
}

/* At this point, Secondary Processor  is "HALT"ed:
        -It booted, but was halted in head.S
        -It was configured to halt-on-reset
       So need to wake it up.
   Essential requirements being where to run from (PC) and stack (SP)
*/
int __cpuinit __cpu_up(unsigned int cpu)
{
    struct task_struct *idle;
    unsigned long wait_till;

    idle = fork_idle(cpu);
    if (IS_ERR(idle)) {
        printk(KERN_ERR "CPU%u: fork() failed\n", cpu);
        return PTR_ERR(idle);
    }

    /* TODO-vineetg: 256 is arbit for now.
       copy_thread saves some things near top of kernel stack.
       Thus leaving some margin for actual stack to begin
       Need to revisit this later
    */
    secondary_boot_data.stack = task_stack_page(idle) + THREAD_SIZE - 256;

    secondary_boot_data.c_entry = first_lines_of_secondary;
    secondary_boot_data.cpu_id = cpu;

    printk(KERN_INFO "Trying to bring up CPU%u ...\n", cpu);

    wakeup_secondary(cpu);

    /* vineetg, Dec 11th 2007, Boot waits for 2nd to come-up
       Wait for 1 sec for 2nd CPU to comeup and then chk it's online bit
       jiffies is incremented every tick and 1 sec has "HZ" num of ticks
       jiffies + HZ => wait for 1 sec
     */

    wait_till = jiffies + HZ;
    while ( time_before(jiffies, wait_till)) {
        if (cpu_online(cpu))
            break;
    }

    if ( !cpu_online(cpu)) {
        printk(KERN_INFO "Timeout: CPU%u FAILED to comeup !!!\n", cpu);
        return -1;
    }

    return 0;
}


/*
 * not supported here
 */
int __init setup_profiling_timer(unsigned int multiplier)
{
    return -EINVAL;
}



/*****************************************************************************/
/*              Inter Processor Interrupt Handling                           */
/*****************************************************************************/


/*
 * structures for inter-processor calls
 * A Collection of single bit ipi messages
 *
 */

//TODO_rajesh investigate timer, tlb and stop message types.
enum ipi_msg_type {
    IPI_RESCHEDULE,
    IPI_CALL_FUNC,
	IPI_CALL_FUNC_SINGLE,
    IPI_CPU_STOP,
};


struct ipi_data {
    spinlock_t lock;
    unsigned long ipi_count;
    unsigned long bits;
};

static DEFINE_PER_CPU(struct ipi_data, ipi_data);

struct smp_call_struct {
    void (*func)(void *info);
    void *info;
    int wait;
    cpumask_t pending;
    cpumask_t unfinished;
};

static struct smp_call_struct* volatile smp_call_function_data;

static void send_ipi_message(const struct cpumask *mask, enum ipi_msg_type msg)
{
    unsigned long flags;
    unsigned int cpu;
    unsigned int k;
    unsigned int this_cpu = smp_processor_id();

    local_irq_save(flags);

    for_each_cpu(cpu, mask) {
        struct ipi_data *ipi = &per_cpu(ipi_data, cpu);

        spin_lock(&ipi->lock);
        ipi->bits |= 1 << msg;
        spin_unlock(&ipi->lock);
    }

    /*
     * Call the platform specific cross-CPU call function.
     */

    for_each_cpu(cpu, mask) {
#ifdef CONFIG_ARCH_ARC800
        idu_irq_assert(cpu);
#else
        k = 1 << cpu;
        (* ( (volatile int*)(REGS_CPU_IPI(this_cpu))))=k;
        (* ( (volatile int*)(REGS_CPU_IPI(this_cpu))))=0;
#endif
    }

    local_irq_restore(flags);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	send_ipi_message(mask, IPI_CALL_FUNC);
}

void arch_send_call_function_single_ipi(int cpu)
{
	send_ipi_message(cpumask_of(cpu), IPI_CALL_FUNC_SINGLE);
}

void smp_send_reschedule(int cpu)
{
    send_ipi_message(cpumask_of(cpu), IPI_RESCHEDULE);
}


void smp_send_stop(void)
{
    cpumask_t mask = cpu_online_map;
    cpu_clear(smp_processor_id(), mask);
    send_ipi_message(&mask, IPI_CPU_STOP);
}

/*
 * ipi_cpu_sto - handle IPI from smp_send_stop()
 *
 */
static void ipi_cpu_stop(unsigned int cpu)
{
    __asm__("flag 1");
}

/*
 * Main handler for inter-processor interrupts
 *
 */

irqreturn_t do_IPI (int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned int cpu = smp_processor_id();
    struct ipi_data *ipi = &per_cpu(ipi_data, cpu);

    ipi->ipi_count++;

#ifdef CONFIG_ARCH_ARC800
    idu_irq_clear((IDU_INTERRUPT_0 + cpu));
#else
    write_new_aux_reg(AUX_IPULSE, (1 << irq));
#endif

    for(;;) {
        unsigned long msgs;

        spin_lock (&ipi->lock);
        msgs = ipi->bits;
        ipi->bits = 0;
        spin_unlock (&ipi->lock);

        if (!msgs)
            break;

        do {
            unsigned long nextmsg;
            nextmsg = msgs & -msgs;
            msgs &= ~nextmsg;


            nextmsg = ffz(~nextmsg);

            switch (nextmsg) {
                case IPI_RESCHEDULE:
               	scheduler_ipi();
                break;

                case IPI_CALL_FUNC:
				generic_smp_call_function_interrupt();
				break;

			case IPI_CALL_FUNC_SINGLE:
				generic_smp_call_function_single_interrupt();
                    break;

                case IPI_CPU_STOP:
                    ipi_cpu_stop (cpu);
                    break;

                default:
                    printk(KERN_CRIT"CPU%u: Unknown IPI message 0x%lx\n",
                            cpu, nextmsg);
                    break;
            }
        } while (msgs);
    }

    return IRQ_HANDLED;
}


#ifdef CONFIG_ARCH_ARC800
static struct irq_node ipi_intr[NR_CPUS];
#endif

void smp_ipi_init(void)
{
	int i;
	unsigned int tmp;
#ifdef CONFIG_ARCH_ARC800
    // Owner of the Idu Interrupt determines who is SELF
    int cpu = smp_processor_id();
#endif

    // Check if CPU is configured for more than 16 interrupts
    // TODO_rajesh: what error should we report at this point
    if(NR_IRQS <=16 || get_hw_config_num_irq() <= 16)
        BUG();

    for (i = 0; i < NR_CPUS; i++)
    {
    	struct ipi_data *ipi = &per_cpu(ipi_data, i);

        spin_lock_init(&ipi->lock);
    }

    // Setup the interrupt in IDU
#ifdef CONFIG_ARCH_ARC800
    idu_disable();
#else
    for (i = 0; i < NR_CPUS; i++)
    {
        tmp = read_new_aux_reg(AUX_IENABLE);
        write_new_aux_reg(AUX_IENABLE, tmp & ~(1 << (i+IPI_IRQS_BASE)));
    }
#endif

#ifdef CONFIG_ARCH_ARC800
    idu_irq_set_tgtcpu(cpu, /* IDU IRQ assoc with CPU */
                        (0x1 << cpu) /* target cpus mask, here single cpu*/
                        );

    idu_irq_set_mode(cpu,  /* IDU IRQ assoc with CPu */
                     IDU_IRQ_MOD_TCPU_ALLRECP,
                     IDU_IRQ_MODE_PULSE_TRIG);

    idu_enable();

    // Install the interrupts
    ipi_intr[cpu].handler = do_IPI;
    ipi_intr[cpu].flags = IRQ_FLG_LOCK;
    ipi_intr[cpu].disable_depth = 0;
    ipi_intr[cpu].dev_id = NULL;
    ipi_intr[cpu].devname = "IPI Interrupt";
    ipi_intr[cpu].next = NULL;

    /* Setup ISR as well as enable IRQ on CPU */
    setup_arc_irq((IDU_INTERRUPT_0 + cpu), &ipi_intr[cpu]);
#else
	for (i = 0; i < NR_CPUS; i++)
	{
		tmp = read_new_aux_reg(AUX_ITRIGGER);
		tmp |= (1 << (i+IPI_IRQS_BASE));
		write_new_aux_reg(AUX_ITRIGGER,tmp);
		tmp = read_new_aux_reg(AUX_IENABLE);
		write_new_aux_reg(AUX_IENABLE, tmp | (1 << (i+IPI_IRQS_BASE)) );
	}
#endif

}

static void
on_each_cpu_mask(void (*func)(void *), void *info, int wait,
               const struct cpumask *mask)
{
       preempt_disable();
       smp_call_function_many(mask, func, info, wait);
       if (cpumask_test_cpu(smp_processor_id(), mask))
       {
    	   local_irq_disable();
    	   func(info);
    	   local_irq_enable();
       }
       preempt_enable();
}

/**********************************************************************/

/*
 * TLB operations
 */
struct tlb_args {
       struct vm_area_struct *ta_vma;
       unsigned long ta_start;
       unsigned long ta_end;
};

static inline void ipi_flush_tlb_all(void *ignored)
{
       local_flush_tlb_all();
}

static inline void ipi_flush_tlb_mm(void *arg)
{
       struct mm_struct *mm = (struct mm_struct *)arg;

       local_flush_tlb_mm(mm);
}

static inline void ipi_flush_tlb_page(void *arg)
{
       struct tlb_args *ta = (struct tlb_args *)arg;

       local_flush_tlb_page(ta->ta_vma, ta->ta_start);
}

static inline void ipi_flush_tlb_range(void *arg)
{
       struct tlb_args *ta = (struct tlb_args *)arg;

       local_flush_tlb_range(ta->ta_vma, ta->ta_start, ta->ta_end);
}

static inline void ipi_flush_tlb_kernel_range(void *arg)
{
       struct tlb_args *ta = (struct tlb_args *)arg;

       local_flush_tlb_kernel_range(ta->ta_start, ta->ta_end);
}

void flush_tlb_all(void)
{
       on_each_cpu(ipi_flush_tlb_all, NULL, 1);
}

void flush_tlb_mm(struct mm_struct *mm)
{
       on_each_cpu_mask(ipi_flush_tlb_mm, mm, 1, mm_cpumask(mm));
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
       struct tlb_args ta;
       ta.ta_vma = vma;
       ta.ta_start = uaddr;
       on_each_cpu_mask(ipi_flush_tlb_page, &ta, 1, mm_cpumask(vma->vm_mm));
}

void flush_tlb_range(struct vm_area_struct *vma,
                     unsigned long start, unsigned long end)
{
       struct tlb_args ta;
       ta.ta_vma = vma;
       ta.ta_start = start;
       ta.ta_end = end;
       on_each_cpu_mask(ipi_flush_tlb_range, &ta, 1, mm_cpumask(vma->vm_mm));
}

void flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
       struct tlb_args ta;
       ta.ta_start = start;
       ta.ta_end = end;
       on_each_cpu(ipi_flush_tlb_kernel_range, &ta, 1);
}
