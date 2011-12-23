/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 */

#include <linux/ptrace.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <asm/arcregs.h>
#include <asm/traps.h>      /* defines for Reg values */
#include <linux/fs_struct.h>
#include <linux/proc_fs.h>  // get_mm_exe_file
#include <linux/file.h>     // fput

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

void show_callee_regs(struct callee_regs *cregs)
{
    print_reg_file(&(cregs->r13), 13);
    printk("\n");
}

void print_task_path_n_nm(struct task_struct *task, char *buf)
{
    struct path path;
    char *nm = NULL;
    struct mm_struct *mm;
    struct file *exe_file;

    mm = get_task_mm(task);
    if (!mm) goto done;

    exe_file = get_mm_exe_file(mm);
    mmput(mm);

    if (exe_file) {
        path = exe_file->f_path;
        path_get(&exe_file->f_path);
        fput(exe_file);
        nm = d_path(&path, buf, 255);
        path_put(&path);
    }

done:
    printk("\ntask = %s '%s', TGID %u PID = %u\n", nm, task->comm,
               task->tgid, task->pid);
}

static void show_faulting_vma(unsigned long address, char *buf)
{
    struct vm_area_struct *vma;
    struct inode *inode;
    unsigned long ino = 0;
    dev_t dev = 0;
    char *nm = buf;

    vma = find_vma(current->active_mm, address);

    /* check against the find_vma( ) behaviour which returns the next VMA
     * if the container VMA is not found
     */
    if (vma && (vma->vm_start <= address)) {
        struct file *file = vma->vm_file;
        if (file) {
                struct path *path = &file->f_path;
                nm = d_path(path, buf, PAGE_SIZE -1);
                inode = vma->vm_file->f_path.dentry->d_inode;
                dev = inode->i_sb->s_dev;
                ino = inode->i_ino;
        }
        printk("@offset 0x%lx in [%s] \nVMA: start 0x%08lx end 0x%08lx\n\n",
               address - vma->vm_start, nm, vma->vm_start, vma->vm_end);
    }
    else printk("@No matching VMA found\n");
}

static void show_ecr_verbose(struct pt_regs *regs)
{
    unsigned int cause_vec, cause_code, cause_reg;
    unsigned long address;

    cause_reg = current->thread.cause_code;
    printk("\n[ECR]: 0x%08x\n", cause_reg);

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
}

/************************************************************************
 *  API called by rest of kernel
 ***********************************************************************/

void show_regs(struct pt_regs *regs)
{
    struct task_struct *tsk = current;
    struct callee_regs *cregs;
    char *buf;

    buf = (char *)__get_free_page(GFP_TEMPORARY);
    if (!buf)
        return;

    print_task_path_n_nm(tsk, buf);

    if (current->thread.cause_code)
        show_ecr_verbose(regs);

    printk("[EFA]: 0x%08lx\n", current->thread.fault_address);
    printk("[ERET]: 0x%08lx (Faulting instruction)\n",regs->ret);

    show_faulting_vma(regs->ret, buf);   // VMA of faulting code, not data

    //extern void print_vma_addr(char *prefix, unsigned long ip);
    //print_vma_addr("",regs->ret);

    /* print special regs */
    printk("status32: 0x%08lx\n", regs->status32);
    printk("SP: 0x%08lx\tFP: 0x%08lx\n", regs->sp, regs->fp);
    printk("BLINK: 0x%08lx\tBTA: 0x%08lx\n",
            regs->blink, regs->bta);
    printk("LPS: 0x%08lx\tLPE: 0x%08lx\tLPC: 0x%08lx \n",
            regs->lp_start, regs->lp_end, regs->lp_count);

    /* print regs->r0 thru regs->r12
     * Sequential printing was generating horrible code
     */
    print_reg_file(&(regs->r0), 0);

    // If Callee regs were saved, display them too
    cregs = (struct callee_regs *) current->thread.callee_reg;
    if (cregs) show_callee_regs(cregs);

    free_page((unsigned long) buf);
}

void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
    unsigned long address, unsigned long cause_reg)
{

    current->thread.fault_address = address;
    current->thread.cause_code = cause_reg;

    // Caller and Callee regs
    show_regs(regs);

    // Show kernel stack trace if this Fatality happened in kernel mode
    if (!user_mode(regs))
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
int irq_inspect_on = 1;   // toggle to switch on/off at runtime

/* Function called from level ISR */
void print_var_on_irq(int irq, int in_or_out, uint addr, uint val)
{
    extern void raw_printk5(const char *str, uint n1, uint n2,
                                uint n3, uint n4);

#ifdef CONFIG_ARC_UART_CONSOLE
    raw_printk5("IRQ \n", irq, in_or_out, addr, val);
#endif
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

u32 clr_on_read = 1;

#ifdef CONFIG_ARC_TLB_PROFILE
u32 numitlb, numdtlb, num_pte_not_present;

static int fill_display_data(char *kbuf)
{
    size_t num = 0;
    num += sprintf(kbuf+num, "I-TLB Miss %x\n", numitlb);
    num += sprintf(kbuf+num, "D-TLB Miss %x\n", numdtlb);
    num += sprintf(kbuf+num, "PTE not present %x\n", num_pte_not_present);

    if (clr_on_read)
        numitlb = numdtlb = num_pte_not_present = 0;

    return num;
}

static int tlb_stats_open(struct inode *inode, struct file *file)
{
    file->private_data = (void *)__get_free_page(GFP_KERNEL);
    return 0;
}

/* called on user read(): display the couters */
static ssize_t tlb_stats_output(
    struct file *file,      /* file descriptor */
    char __user *user_buf,  /* user buffer */
    size_t  len,            /* length of buffer */
    loff_t *offset)         /* offset in the file */
{
    size_t num;
    char *kbuf = (char *)file->private_data;

    num = fill_display_data(kbuf);

   /* simple_read_from_buffer() is helper for copy to user space
     It copies up to @2 (num) bytes from kernel buffer @4 (kbuf) at offset
     @3 (offset) into the user space address starting at @1 (user_buf).
     @5 (len) is max size of user buffer
   */
    return simple_read_from_buffer(user_buf, num, offset, kbuf, len);
}

/* called on user write : clears the counters */
static ssize_t tlb_stats_clear(struct file *file, const char __user *user_buf,
     size_t length, loff_t *offset)
{
    numitlb = numdtlb = num_pte_not_present = 0;
    return  length;
}


static int tlb_stats_close(struct inode *inode, struct file *file)
{
    free_page((unsigned long)(file->private_data));
    return 0;
}

static struct file_operations tlb_stats_file_ops = {
    .read = tlb_stats_output,
    .write = tlb_stats_clear,
    .open = tlb_stats_open,
    .release = tlb_stats_close
};
#endif

static int __init arc_debugfs_init(void)
{
    test_dir = debugfs_create_dir("arc", NULL);

#ifdef CONFIG_ARC_TLB_PROFILE
    test_dentry = debugfs_create_file("tlb_stats", 0444, test_dir, NULL,
                            &tlb_stats_file_ops);
#endif

    test_u32_dentry = debugfs_create_u32("clr_on_read", 0444, test_dir, &clr_on_read);

    return 0;
}
module_init(arc_debugfs_init);

static void __exit arc_debugfs_exit(void)
{
    debugfs_remove(test_u32_dentry);
    debugfs_remove(test_dentry);
    debugfs_remove(test_dir);
}

#endif
