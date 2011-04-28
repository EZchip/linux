/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 *  Miscll Diagnostics and Troubleshooting routines
 *
 *****************************************************************************/
#include <linux/ptrace.h>
#include <linux/module.h>
#include <asm/arcregs.h>
#include <asm/traps.h>      /* defines for Reg values */
#include <linux/kallsyms.h>

static void show_ecr_verbose(struct pt_regs *regs);
void show_callee_regs(struct callee_regs *cregs);

/* For dumping register file (r0-r12) or (r13-r25), instead of 13 printks,
 * we simply loop otherwise gcc generates 13 calls to printk each with it's
 * own arg setup
 */
void noinline print_reg_file(unsigned long *last_reg, int num)
{
    unsigned int i;

    for (i = num; i < num + 13; i++) {
        printk("r%02u: 0x%08lx\t", i, *last_reg);
        if ( ( (i+1) % 3) == 0 ) printk("\n");
        last_reg--;
    }
}

void show_regs(struct pt_regs *regs)
{
    struct callee_regs *cregs;

    if (current->thread.cause_code)
        show_ecr_verbose(regs);

    /* print special regs */
    printk("status32: 0x%08lx\n", regs->status32);
    printk("sp: 0x%08lx\tfp: 0x%08lx\n", regs->sp, regs->fp);
    printk("ret: 0x%08lx\tblink: 0x%08lx\tbta: 0x%08lx\n",
            regs->ret, regs->blink, regs->bta);
    printk("LPS: 0x%08lx\tLPE: 0x%08lx\tLPC: 0x%08lx \n",
            regs->lp_start, regs->lp_end, regs->lp_count);

    /* print regs->r0 thru regs->r12
     * Sequential printing was generating horrible code
     */
    print_reg_file(&(regs->r0), 0);

    // If Callee regs were saved, display them too
    cregs = (struct callee_regs *) current->thread.callee_reg;
    if (cregs) show_callee_regs(cregs);
}

void show_callee_regs(struct callee_regs *cregs)
{
    print_reg_file(&(cregs->r13), 13);
    printk("\n");
}

/************************************************************************
 *  Verbose Display of Exception
 ***********************************************************************/

static void show_ecr_verbose(struct pt_regs *regs)
{
    unsigned int cause_vec, cause_code, cause_reg;
    unsigned long address;
    struct task_struct *tsk = current;

    printk("Current task = '%s', PID = %u, ASID = %lu\n", tsk->comm,
               tsk->pid, tsk->active_mm->context.asid);

    cause_reg = current->thread.cause_code;

     /* For Data fault, this is data address not instruction addr */
    address = current->thread.fault_address;

    cause_vec = cause_reg >> 16;
    cause_code = ( cause_reg >> 8 ) & 0xFF;

    /* For DTLB Miss or ProtV, display the memory involved too */
    if ( cause_vec == 0x22)   // DTLB Miss
    {
        if (cause_code != 0x04 ) {  // Mislaigned access doesn't tell R/W/X
            printk("While (%s): 0x%08lx by instruction @ 0x%08lx\n",
                ((cause_code == 0x01)?"Read From":
                ((cause_code == 0x02)?"Write to":"Exchg")),
                address, regs->ret);
        }
    }
    else if (cause_vec == 0x20) {  /* Machine Check */
        printk("Reason: (%s)\n",
            (cause_code == 0x0)?"Double Fault":"Other Fatal Err");
    }
    else if (cause_vec == PROTECTION_VIOL) {
        printk("Reason : ");
        if (cause_code == 0x0)
            printk("Instruction fetch protection violation (execute from page marked non-execute)\n");
        else if (cause_code == 0x1)
            printk("Data read protection violation (read from page marked non-read)\n");
        else if (cause_code == 0x2)
            printk("Data store protection violation (write to a page marked non-write)\n");
        else if (cause_code == 0x3)
            printk("Data exchange protection violation\n");
        else if (cause_code ==0x4)
            printk("Misaligned access\n");

    }


    printk("\n[ECR]: 0x%08x\n", cause_reg);
    printk("[EFA]: 0x%08lx\n", address);
}


void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
    unsigned long address, unsigned long cause_reg)
{

    // Caller and Callee regs
    show_regs(regs);

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
