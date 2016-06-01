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
#include <linux/fs_struct.h>
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <asm/arcregs.h>
#include <asm/irqflags.h>
#ifdef CONFIG_ARC_PLAT_EZNPS
#include <plat/smp.h>
#endif

/*
 * Common routine to print scratch regs (r0-r12) or callee regs (r13-r25)
 *   -Prints 3 regs per line and a CR.
 *   -To continue, callee regs right after scratch, special handling of CR
 */
static noinline void print_reg_file(long *reg_rev, int start_num)
{
	unsigned int i;
	char buf[512];
	int n = 0, len = sizeof(buf);

	for (i = start_num; i < start_num + 13; i++) {
		n += scnprintf(buf + n, len - n, "r%02u: 0x%08lx\t",
			       i, (unsigned long)*reg_rev);

		if (((i + 1) % 3) == 0)
			n += scnprintf(buf + n, len - n, "\n");

		/* because pt_regs has regs reversed: r12..r0, r25..r13 */
		if (is_isa_arcv2() && start_num == 0)
			reg_rev++;
		else
			reg_rev--;
	}

	if (start_num != 0)
		n += scnprintf(buf + n, len - n, "\n\n");

	/* To continue printing callee regs on same line as scratch regs */
	if (start_num == 0)
		pr_info("%s", buf);
	else
		pr_cont("%s\n", buf);
}

static void show_callee_regs(struct callee_regs *cregs)
{
	print_reg_file(&(cregs->r13), 13);
}

#ifdef CONFIG_ARC_PLAT_EZNPS
static int print_memory_exception(void)
{
	long long int memory_address;
	unsigned int cpu, ciu_block_id, err_cap_1_value, err_sts_value;
	unsigned int *err_cap_1_ptr, *err_cap_2_ptr, *err_sts_ptr;
	struct nps_ciu_err_cap_2 err_cap_2;
	
	/* getting the registers values */
	cpu = smp_processor_id();
	ciu_block_id = NPS_CPU_TO_CORE_NUM(cpu) < 
		NPS_CLUSTER_MIDDLE_CORE_NUMBER ? NPS_CIU_WEST_BLOCK_ID : 
		NPS_CIU_EAST_BLOCK_ID;
		
	err_cap_1_ptr = nps_host_reg(cpu, ciu_block_id, NPS_CIU_ERR_CAP_1);
	err_cap_1_value = *err_cap_1_ptr;
	
	err_cap_2_ptr = nps_host_reg(cpu, ciu_block_id,	NPS_CIU_ERR_CAP_2);
	err_cap_2.value = *err_cap_2_ptr;
	
	err_sts_ptr = nps_host_reg(cpu, ciu_block_id, NPS_CIU_ERR_STS);
	err_sts_value = *err_sts_ptr;
	
	/* returning 1 if the error is not caused by memory exception */
	if (err_sts_value == 0)
		return 1;

	/* the 35th and 34th bits of the memory address are the first
	 * two bits of ERR_CAP_2, bits 33 to 3 are contained in 
	 * ERR_CAP_1 and the first two bits will be zero
	*/
	memory_address = (err_cap_2.addr <<
		ERR_CAP_2_ADDRESS_SHIFT_LEFT_VALUE) | (err_cap_1_value <<
		ERR_CAP_1_SHIFT_LEFT_VALUE);

	pr_cont("Memory error exception");
	pr_info("          MSID = 0x%lx\n", err_cap_2.msid);
	pr_info("          Physical address = 0x%llx", memory_address);
	pr_info("          Error code = 0x%lx => ", err_cap_2.erc);
	
	switch(err_cap_2.erc) {
	case ERR_CAP_2_ERC_INVALID_MSID:
		pr_cont("Invalid MSID");
		break;
	case ERR_CAP_2_ERC_ADDR_RANGE_VIOLATION:
		pr_cont("Address range violation");
		break;
	case ERR_CAP_2_ERC_ALIGNMENT_SIZE:
		pr_cont("Alignment/Size error");
		break;
	case ERR_CAP_2_ERC_BAD_TRANSACTION:
		pr_cont("Bad transaction");
		break;
	case ERR_CAP_2_ERC_ILLEGAL_FIELD_VALUE:
		pr_cont("Illegal field value");
		break;
	case ERR_CAP_2_ERC_WRITE_PRIVILAGE_VIOLATION:
		pr_cont("Write protection privilege violation");
		break;
	case ERR_CAP_2_ERC_UNCORRECTABLE_ECC:
		pr_cont("Uncorrectable ECC data error");
		break;
	default: 
		pr_cont("Unknown error");
	}

	pr_info("          Transaction code = 0x%lx => ", err_cap_2.rqtc);
	
	switch(err_cap_2.rqtc) {
	case ERR_CAP_2_RQTC_POST_WRITE:
		pr_cont("Posted write-single or atomic write-update");
		break;
	case ERR_CAP_2_RQTC_NON_POST_WRITE:
		pr_cont("Non-posted write-single or atomic write-update");
		break;
	case ERR_CAP_2_RQTC_POST_WIDE_WRITE:
		pr_cont("Posted wide write-single or wide atomic write-update");
		break;
	case ERR_CAP_2_RQTC_NON_POST_WIDE_WRITE:
		pr_cont("Non-posted wide write-single or wide atomic write-" \
					"update");
		break;
	case ERR_CAP_2_RQTC_BURST_WRITE:
		pr_cont("Burst write");
		break;
	case ERR_CAP_2_RQTC_BURST_WRITE_SAFE:
		pr_cont("Burst write-safe");
		break;
	case ERR_CAP_2_RQTC_ADDR_BASE_MAINTENANCE:
		pr_cont("Address based maintenance commands");
		break;
	case ERR_CAP_2_RQTC_SINGLE_READ:
		pr_cont("Single read / atomic read-update");
		break;
	case ERR_CAP_2_RQTC_BURST_READ:
		pr_cont("Burst read");
		break;
	case ERR_CAP_2_RQTC_BURST_L2_READ:
		pr_cont("Burst read with L2 cache line");
		break;
	default:
		pr_cont("Unknown RQTC");
	}
		
	return 0;
}
#endif

static void print_task_path_n_nm(struct task_struct *tsk, char *buf)
{
	char *path_nm = NULL;
	struct mm_struct *mm;
	struct file *exe_file;

	mm = get_task_mm(tsk);
	if (!mm)
		goto done;

	exe_file = get_mm_exe_file(mm);
	mmput(mm);

	if (exe_file) {
		path_nm = file_path(exe_file, buf, 255);
		fput(exe_file);
	}

done:
	pr_info("Path: %s\n", !IS_ERR(path_nm) ? path_nm : "?");
}

static void show_faulting_vma(unsigned long address, char *buf)
{
	struct vm_area_struct *vma;
	struct inode *inode;
	unsigned long ino = 0;
	dev_t dev = 0;
	char *nm = buf;
	struct mm_struct *active_mm = current->active_mm;

	/* can't use print_vma_addr() yet as it doesn't check for
	 * non-inclusive vma
	 */
	down_read(&active_mm->mmap_sem);
	vma = find_vma(active_mm, address);

	/* check against the find_vma( ) behaviour which returns the next VMA
	 * if the container VMA is not found
	 */
	if (vma && (vma->vm_start <= address)) {
		struct file *file = vma->vm_file;
		if (file) {
			nm = file_path(file, buf, PAGE_SIZE - 1);
			inode = file_inode(vma->vm_file);
			dev = inode->i_sb->s_dev;
			ino = inode->i_ino;
		}
		pr_info("    @off 0x%lx in [%s]\n"
			"    VMA: 0x%08lx to 0x%08lx\n",
			vma->vm_start < TASK_UNMAPPED_BASE ?
				address : address - vma->vm_start,
			nm, vma->vm_start, vma->vm_end);
	} else
		pr_info("    @No matching VMA found\n");

	up_read(&active_mm->mmap_sem);
}

static void show_ecr_verbose(struct pt_regs *regs)
{
	unsigned int vec, cause_code;
	unsigned long address;

	pr_info("\n[ECR   ]: 0x%08lx => ", regs->event);

	/* For Data fault, this is data address not instruction addr */
	address = current->thread.fault_address;

	vec = regs->ecr_vec;
	cause_code = regs->ecr_cause;

	/* For DTLB Miss or ProtV, display the memory involved too */
	if (vec == ECR_V_DTLB_MISS) {
		pr_cont("Invalid %s @ 0x%08lx by insn @ 0x%08lx\n",
		       (cause_code == 0x01) ? "Read" :
		       ((cause_code == 0x02) ? "Write" : "EX"),
		       address, regs->ret);
	} else if (vec == ECR_V_ITLB_MISS) {
		pr_cont("Insn could not be fetched\n");
	} else if (vec == ECR_V_MACH_CHK) {
		if (cause_code == 0x0)
			pr_cont("Double Fault");
		else {
#ifdef CONFIG_ARC_PLAT_EZNPS
			if (print_memory_exception())
				pr_cont("Other fatal error\n");
#else
			pr_cont("Other fatal error\n");
#endif
		}
	} else if (vec == ECR_V_PROTV) {
		if (cause_code == ECR_C_PROTV_INST_FETCH)
			pr_cont("Execute from Non-exec Page\n");
		else if (cause_code == ECR_C_PROTV_MISALIG_DATA)
			pr_cont("Misaligned r/w from 0x%08lx\n", address);
		else
			pr_cont("%s access not allowed on page\n",
				(cause_code == 0x01) ? "Read" :
				((cause_code == 0x02) ? "Write" : "EX"));
	} else if (vec == ECR_V_INSN_ERR) {
		pr_cont("Illegal Insn\n");
#ifdef CONFIG_ISA_ARCV2
	} else if (vec == ECR_V_MEM_ERR) {
		if (cause_code == 0x00)
			pr_cont("Bus Error from Insn Mem\n");
		else if (cause_code == 0x10)
			pr_cont("Bus Error from Data Mem\n");
		else
			pr_cont("Bus Error, check PRM\n");
#endif
	} else {
		pr_cont("Check Programmer's Manual\n");
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
	show_regs_print_info(KERN_INFO);

	show_ecr_verbose(regs);

	pr_info("[EFA   ]: 0x%08lx\n[BLINK ]: %pS\n[ERET  ]: %pS\n",
		current->thread.fault_address,
		(void *)regs->blink, (void *)regs->ret);

	if (user_mode(regs))
		show_faulting_vma(regs->ret, buf); /* faulting code, not data */

	pr_info("[STAT32]: 0x%08lx", regs->status32);

#define STS_BIT(r, bit)	r->status32 & STATUS_##bit##_MASK ? #bit" " : ""

#ifdef CONFIG_ISA_ARCOMPACT
	pr_cont(" : %2s%2s%2s%2s%2s%2s%2s\n",
			(regs->status32 & STATUS_U_MASK) ? "U " : "K ",
			STS_BIT(regs, DE), STS_BIT(regs, AE),
			STS_BIT(regs, A2), STS_BIT(regs, A1),
			STS_BIT(regs, E2), STS_BIT(regs, E1));
#else
	pr_cont(" : %2s%2s%2s%2s\n",
			STS_BIT(regs, IE),
			(regs->status32 & STATUS_U_MASK) ? "U " : "K ",
			STS_BIT(regs, DE), STS_BIT(regs, AE));
#endif
	pr_info("BTA: 0x%08lx\t SP: 0x%08lx\t FP: 0x%08lx\n",
		regs->bta, regs->sp, regs->fp);
	pr_info("LPS: 0x%08lx\tLPE: 0x%08lx\tLPC: 0x%08lx\n",
	       regs->lp_start, regs->lp_end, regs->lp_count);

	/* print regs->r0 thru regs->r12
	 * Sequential printing was generating horrible code
	 */
	print_reg_file(&(regs->r0), 0);

	/* If Callee regs were saved, display them too */
	cregs = (struct callee_regs *)current->thread.callee_reg;
	if (cregs)
		show_callee_regs(cregs);

	free_page((unsigned long)buf);
}

void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
			    unsigned long address)
{
	current->thread.fault_address = address;

	/* Caller and Callee regs */
	show_regs(regs);

	/* Show stack trace if this Fatality happened in kernel mode */
	if (!user_mode(regs))
		show_stacktrace(current, regs);
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

static u32 clr_on_read = 1;

#ifdef CONFIG_ARC_DBG_TLB_MISS_COUNT
u32 numitlb, numdtlb, num_pte_not_present;

static int fill_display_data(char *kbuf)
{
	size_t num = 0;
	num += sprintf(kbuf + num, "I-TLB Miss %x\n", numitlb);
	num += sprintf(kbuf + num, "D-TLB Miss %x\n", numdtlb);
	num += sprintf(kbuf + num, "PTE not present %x\n", num_pte_not_present);

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
static ssize_t tlb_stats_output(struct file *file,	/* file descriptor */
				char __user *user_buf,	/* user buffer */
				size_t len,		/* length of buffer */
				loff_t *offset)		/* offset in the file */
{
	size_t num;
	char *kbuf = (char *)file->private_data;

	/* All of the data can he shoved in one iteration */
	if (*offset != 0)
		return 0;

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
	return length;
}

static int tlb_stats_close(struct inode *inode, struct file *file)
{
	free_page((unsigned long)(file->private_data));
	return 0;
}

static const struct file_operations tlb_stats_file_ops = {
	.read = tlb_stats_output,
	.write = tlb_stats_clear,
	.open = tlb_stats_open,
	.release = tlb_stats_close
};
#endif

static int __init arc_debugfs_init(void)
{
	test_dir = debugfs_create_dir("arc", NULL);

#ifdef CONFIG_ARC_DBG_TLB_MISS_COUNT
	test_dentry = debugfs_create_file("tlb_stats", 0444, test_dir, NULL,
					  &tlb_stats_file_ops);
#endif

	test_u32_dentry =
	    debugfs_create_u32("clr_on_read", 0444, test_dir, &clr_on_read);

	return 0;
}

module_init(arc_debugfs_init);

static void __exit arc_debugfs_exit(void)
{
	debugfs_remove(test_u32_dentry);
	debugfs_remove(test_dentry);
	debugfs_remove(test_dir);
}
module_exit(arc_debugfs_exit);

#endif
	
