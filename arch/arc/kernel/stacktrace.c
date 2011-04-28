/******************************************************************************
 * Copyright ARC International (www.arc.com) 2008-2010
 *
 *              Stack tracing for ARC Linux
 *
 *
 *  vineetg: March 2009
 *  -Implemented correct versions of thread_saved_pc() and get_wchan()
 *
 *  rajeshwarr: 2008
 *  -Initial implementation
 *****************************************************************************/
#include <linux/ptrace.h>
#include <linux/module.h>
#include <asm/arcregs.h>
#include <asm/unwind.h>
#include <asm/utils.h>

/* Ofcourse just returning schedule( ) would be pointless so unwind until
 * the function is not in schedular code
 */

/* If tsk is null, display the stack trace of the current task
 */
void show_stacktrace(struct task_struct *tsk, struct pt_regs *regs)
{
#ifdef CONFIG_ARC_STACK_UNWIND
    struct unwind_frame_info frame_info;

    if (tsk == NULL)
    {
        unsigned long fp, sp, blink, ret;
        frame_info.task = current;

        __asm__ __volatile__(
                  "1:mov %0,r27\n\t"
                    "mov %1,r28\n\t"
                    "mov %2,r31\n\t"
                    "mov %3,r63\n\t"
                   :"=r"(fp),"=r"(sp),"=r"(blink),"=r"(ret)
                   :
                   );

        frame_info.regs.r27 = fp;
        frame_info.regs.r28 = sp;
        frame_info.regs.r31 = blink;
        frame_info.regs.r63 = ret;
        frame_info.call_frame = 0;
    }
    else
    {
        frame_info.task = tsk;

        frame_info.regs.r27 = regs->fp;
        frame_info.regs.r28 = regs->sp;
        frame_info.regs.r31 = regs->blink;
        frame_info.regs.r63 = regs->ret;
        frame_info.call_frame = 0;
    }

    printk("\nStack Trace:\n");

    while(1)
    {
        char namebuf[KSYM_NAME_LEN+1];
        unsigned long address;
        int ret = 0;

        address = UNW_PC(&frame_info);
        if(address) {
#ifdef CONFIG_KALLSYMS
            printk("  %#lx :", address);
#endif
            // if KALLSYMS prints name, else prints just addr
            printk("  %s\n",
                arc_identify_sym_r(address, namebuf, sizeof(namebuf)));
        }

        ret = arc_unwind(&frame_info);

        if(ret == 0) {
            frame_info.regs.r63 = frame_info.regs.r31;
            continue;
        }
        else {
            break;
        }
    }
#else
    printk("CONFIG_ARC_STACK_UNWIND needs to be enabled\n");
#endif
    return;
}
EXPORT_SYMBOL(show_stacktrace);


/*
 * Expected by sched Code
 */
void show_stack(struct task_struct *tsk, unsigned long *sp)
{
    show_stacktrace(0,0);
    sort_snaps(1);
}

/*
 * Expected by Rest of kernel code
 */
void dump_stack(void)
{
    show_stacktrace(0,0);
}


/*
 * API: expected by schedular Code: If thread is sleeping where is that.
 * What is this good for? it will be always the scheduler or ret_from_fork.
 */
unsigned long thread_saved_pc(struct task_struct *t)
{
    struct pt_regs *regs = task_pt_regs(t);
    struct unwind_frame_info frame_info;
    unsigned long address;

    frame_info.task = t;

    frame_info.regs.r27 = regs->fp;
    frame_info.regs.r28 = regs->sp;
    frame_info.regs.r31 = regs->blink;
    frame_info.regs.r63 = regs->ret;
    frame_info.call_frame = 0;

    address = UNW_PC(&frame_info);

    return address;
}

unsigned int get_wchan(struct task_struct *p)
{
    return 0;
}
