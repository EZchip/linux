/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 *  Miscll Diagnostics and Troubleshooting routines
 *
 *****************************************************************************/
#include <linux/ptrace.h>
#include <linux/module.h>
#include <asm/arcregs.h>
#include <asm/utils.h>

void show_regs(struct pt_regs *regs)
{
    unsigned int i = 0;
    printk("sp: 0x%08lx\tfp: 0x%08lx\n", regs->sp, regs->fp);
    printk("ret: 0x%08lx\tblink: 0x%08lx\n", regs->ret, regs->blink);
    printk("status32: 0x%08lx\n", regs->status32);
    printk("r%02u: %08lx\t", i++, regs->r0);
    printk("r%02u: %08lx\t", i++, regs->r1);
    printk("r%02u: %08lx\t", i++, regs->r2);
    printk("\n");
    printk("r%02u: %08lx\t", i++, regs->r3);
    printk("r%02u: %08lx\t", i++, regs->r4);
    printk("r%02u: %08lx\t", i++, regs->r5);
    printk("\n");
    printk("r%02u: %08lx\t", i++, regs->r6);
    printk("r%02u: %08lx\t", i++, regs->r7);
    printk("r%02u: %08lx\t", i++, regs->r8);
    printk("\n");
    printk("r%02u: %08lx\t", i++, regs->r9);
    printk("r%02u: %08lx\t", i++, regs->r10);
    printk("r%02u: %08lx\t", i++, regs->r11);
    printk("\n");
    printk("r%02u: %08lx\t", i++, regs->r12);
}

void show_callee_regs(struct callee_regs *cregs)
{
    unsigned int i = 13;
    printk("r%02u: %08lx\t", i++, cregs->r13);
    printk("r%02u: %08lx\t", i++, cregs->r14);
    printk("\n");
    printk("r%02u: %08lx\t", i++, cregs->r15);
    printk("r%02u: %08lx\t", i++, cregs->r16);
    printk("r%02u: %08lx\t", i++, cregs->r17);
    printk("\n");
    printk("r%02u: %08lx\t", i++, cregs->r18);
    printk("r%02u: %08lx\t", i++, cregs->r19);
    printk("r%02u: %08lx\t", i++, cregs->r20);
    printk("\n");
    printk("r%02u: %08lx\t", i++, cregs->r21);
    printk("r%02u: %08lx\t", i++, cregs->r22);
    printk("r%02u: %08lx\t", i++, cregs->r23);
    printk("\n");
    printk("r%02u: %08lx\t", i++, cregs->r24);
    printk("r%02u: %08lx\t", i++, cregs->r25);
    printk("\n\n");
}

void show_fault_diagnostics(const char *str, struct pt_regs *regs,
    struct callee_regs *cregs, unsigned long address, unsigned long cause_reg)
{
    struct task_struct *tsk = current;
    int cause_vec, cause_code;

    printk("%s\n[ECR]: 0x%08lx\n", str, cause_reg);
    printk("Faulting Instn : 0x%08lx\n", regs->ret);

    cause_vec = cause_reg >> 16;
    cause_code = ( cause_reg >> 8 ) && 0xFF;

    /* For DTLB Miss or ProtV, display the memory involved too */
    if ( (cause_vec == 0x22) ||  // DTLB Miss
         (cause_vec == 0x23) )   // ProtV
    {
		printk("While (%s): 0x%08lx\n",((cause_code == 0x01)?"Read From":
                                        ((cause_code == 0x020)?"Write to":"Exchg")),
                                         address);
    }
    else if (cause_vec == 0x20) {  /* Machine Check */
        printk("Reason: (%s)\n", (cause_code == 0x0)?"Double Fault":"Other Fatal Err");
    }

    printk("Current task = '%s', PID = %u, ASID = %lu\n\n", tsk->comm,
               tsk->pid, tsk->active_mm->context.asid);

    // Basic Regs are always available
    show_regs(regs);

    // If we have Calle regs, display them too
    if (cregs) show_callee_regs(cregs);

    // Show kernel stack trace if this Fatality happened in kernel mode
    if (!(regs->status32 & STATUS_U_MASK))
        show_stacktrace(current, regs);

    sort_snaps(1);
}


/*
 * Monitoring a Variable every IRQ entry/exit
 * Low Level ISR can code to dump the contents of a variable
 * This can for e.g. be used to figure out the whether the @var
 * got clobbered during ISR or between ISRs (pure kernel mode)
 *
 * The macro itself can be switched on/off at runtime using a toggle
 * @irq_inspect_on
 */
extern void raw_printk5(const char *str, uint n1, uint n2, uint n3, uint n4);

int irq_inspect_on = 1;   // toggle to switch on/off at runtime

/* Function called from level ISR */
void print_var_on_irq(int irq, int in_or_out, uint addr, uint val)
{
    raw_printk5("IRQ \n", irq, in_or_out, addr, val);
}

#ifdef CONFIG_DEBUG_FS

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/debugfs.h>


static struct dentry *test_dentry;
static struct dentry *test_dir;
static struct dentry *test_u32_dentry;
static u32 value = 42;

static ssize_t example_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];

	sprintf(buf, "foola\n");
	return simple_read_from_buffer(user_buf, count, ppos, buf, sizeof(buf));
}

static int example_open(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations example_file_operations = {
	.read = example_read_file,
	.open = example_open,
};

static int __init example_init(void)
{
	test_dir = debugfs_create_dir("foo_dir", NULL);

	test_dentry = debugfs_create_file("foo", 0444, test_dir, NULL,
							&example_file_operations);

	test_u32_dentry = debugfs_create_u32("foo_u32", 0444, test_dir, &value);

	return 0;
}
module_init(example_init);

static void __exit example_exit(void)
{
	debugfs_remove(test_u32_dentry);
	debugfs_remove(test_dentry);
	debugfs_remove(test_dir);
}

#endif
