/*
* Copyright (c) 2016, Mellanox Technologies. All rights reserved.
*
* This software is available to you under a choice of one of two
* licenses.  You may choose to be licensed under the terms of the GNU
* General Public License (GPL) Version 2, available from the file
* COPYING in the main directory of this source tree, or the
* OpenIB.org BSD license below:
*
*     Redistribution and use in source and binary forms, with or
*     without modification, are permitted provided that the following
*     conditions are met:
*
*      - Redistributions of source code must retain the above
*        copyright notice, this list of conditions and the following
*        disclaimer.
*
*      - Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following
*        disclaimer in the documentation and/or other materials
*        provided with the distribution.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <asm/dpl.h>
#include <plat/ctop.h>
#include <plat/perf_event.h>

DEFINE_PER_CPU(pid_t, dp_running_pid);

struct dpl_arg {
	void __iomem *addr;
	void *buf;
	int len;
	int write;
	unsigned int dpc;
};

struct dpl_mmap_arg {
	struct task_struct *task;
	resource_size_t user_base;
	void __iomem *kernel_base;
};


static bool dpl_is_valid_aux_address(void *aux_reg_address)
{
	if ((unsigned int)aux_reg_address < CTOP_AUX_BASE)
		return false;

	return true;
}


static bool dpl_is_valid_dp_address(void *reg_db_address)
{
	if ((unsigned int)reg_db_address < NPS_HOST_REG_BASE)
		return false;

	return true;
}

static bool dpl_is_valid_pct_address(void *pct_reg_address)
{
	unsigned int addr = (unsigned int) pct_reg_address;

	return ((addr == NPS_REG_PCT_COUNT0) ||
		(addr == NPS_REG_PCT_COUNT1) ||
		(addr == NPS_REG_PCT_CONFIG0) ||
		(addr == NPS_REG_PCT_CONFIG1) ||
		(addr == NPS_REG_PCT_CONTROL));
}

static long dpl_set_aux(struct dpl_aux_reg __user *user_reg)
{
	struct dpl_aux_reg kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_aux_reg)))
		return -EFAULT;

	if (!dpl_is_valid_aux_address((void *)kernel_reg.address))
		return -EFAULT;

	write_aux_reg(kernel_reg.address, kernel_reg.value);

	return 0;
}

static long dpl_get_aux(struct dpl_aux_reg __user *user_reg)
{
	struct dpl_aux_reg kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_aux_reg)))
			return -EFAULT;

	if (!dpl_is_valid_aux_address((void *)kernel_reg.address))
		return -EFAULT;

	kernel_reg.value = read_aux_reg(kernel_reg.address);
	if (copy_to_user(user_reg, &kernel_reg, sizeof(struct dpl_aux_reg)))
		return -EFAULT;

	return 0;
}

static long dpl_set_reg_db(struct dpl_reg_db __user *user_reg)
{
	struct dpl_reg_db kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_reg_db)))
		return -EFAULT;

	if (!dpl_is_valid_dp_address((void *)kernel_reg.address))
		return -EFAULT;

	*(unsigned int *)kernel_reg.address = kernel_reg.value;

	return 0;
}

static long dpl_get_reg_db(struct dpl_reg_db __user *user_reg)
{
	struct dpl_reg_db kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_reg_db)))
		return -EFAULT;

	if (!dpl_is_valid_dp_address((void *)kernel_reg.address))
		return -EFAULT;

	kernel_reg.value = *(unsigned int *)kernel_reg.address;
	if (copy_to_user(user_reg, &kernel_reg, sizeof(struct dpl_reg_db)))
		return -EFAULT;

	return 0;
}

static long dpl_set_pct_reg(struct dpl_aux_reg __user *user_reg)
{
	struct dpl_aux_reg kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_aux_reg)))
		return -EFAULT;

	if (!dpl_is_valid_pct_address((void *)kernel_reg.address))
		return -EFAULT;

	write_aux_reg(kernel_reg.address, kernel_reg.value);

	return 0;
}

static long dpl_get_pct_reg(struct dpl_aux_reg __user *user_reg)
{
	struct dpl_aux_reg kernel_reg;

	if (copy_from_user(&kernel_reg, user_reg, sizeof(struct dpl_aux_reg)))
			return -EFAULT;

	if (!dpl_is_valid_pct_address((void *)kernel_reg.address))
		return -EFAULT;

	kernel_reg.value = read_aux_reg(kernel_reg.address);
	if (copy_to_user(user_reg, &kernel_reg, sizeof(struct dpl_aux_reg)))
		return -EFAULT;

	return 0;
}

static long dpl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret_val = 0;

	switch (cmd) {
	case DPL_SET_AUX_REG:
		ret_val = dpl_set_aux((struct dpl_aux_reg __user *)arg);
		break;

	case DPL_GET_AUX_REG:
		ret_val = dpl_get_aux((struct dpl_aux_reg __user *)arg);
		break;

	case DPL_SET_REG_DB:
		ret_val = dpl_set_reg_db((struct dpl_reg_db __user *)arg);
		break;

	case DPL_GET_REG_DB:
		ret_val = dpl_get_reg_db((struct dpl_reg_db __user *)arg);
		break;

	case DPL_SET_PCT_REG:
		ret_val = dpl_set_pct_reg((struct dpl_aux_reg __user *)arg);
		break;

	case DPL_GET_PCT_REG:
		ret_val = dpl_get_pct_reg((struct dpl_aux_reg __user *)arg);
		break;

	default:
		return -ENOTTY;
	}

	return ret_val;
}

static void dpl_access_phys_local(void *arg)
{
	struct dpl_arg *a = (struct dpl_arg *)arg;

	if (arg == NULL)
		return;

	write_aux_reg(CTOP_AUX_DPC, a->dpc);
	if (a->write) {
		memcpy_toio(a->addr, a->buf, a->len);
		/* if the address is cached there is a need
		 * to flush dcache so that the data will be
		 * in memory. also there is a need to flush
		 * icache as the the data could be
		 * executable code
		 */
		flush_cache_all();
	} else {
		memcpy_fromio(a->buf, a->addr, a->len);
	}
	write_aux_reg(CTOP_AUX_DPC, current->thread.dp.dpc);
}

static int dpl_access_phys(struct vm_area_struct *vma, unsigned long addr,
		void *buf, int len, int write)
{
	struct dpl_mmap_arg *mmap_arg;
	int cpu, offset;
	struct dpl_arg arg = {
			.buf = buf,
			.len = len,
			.write = write,
	};

	if (vma == NULL)
		return -EINVAL;

	if (vma->vm_private_data == NULL)
		return -EINVAL;

	mmap_arg = vma->vm_private_data;
	arg.dpc = mmap_arg->task->thread.dp.dpc;
	offset = addr - mmap_arg->user_base;
	arg.addr = mmap_arg->kernel_base + offset;
	cpu = task_cpu(mmap_arg->task);

	preempt_disable();
	smp_call_function_single(cpu, dpl_access_phys_local, &arg, 1);
	preempt_enable();

	return len;
}

static void dpl_vma_close(struct vm_area_struct *area)
{
	struct dpl_mmap_arg *mmap_arg = area->vm_private_data;

	kfree(mmap_arg);
}

static const struct vm_operations_struct dpl_mem_ops = {
	.access = dpl_access_phys,
	.close = dpl_vma_close,
};

static int dpl_mmap(struct file *file, struct vm_area_struct *vma)
{
	resource_size_t phys_addr = vma->vm_pgoff << PAGE_SHIFT;
	struct dpl_mmap_arg *arg;

	if (io_remap_pfn_range(vma, vma->vm_start, max_pfn + 1,
			       vma->vm_end - vma->vm_start,
			       PAGE_U_NONE))
		return -EAGAIN;

	/* This area should be part of core dump */
	vma->vm_flags &= ~(VM_IO | VM_DONTDUMP);

	vma->vm_ops = &dpl_mem_ops;

	arg = kmalloc(sizeof(*arg), GFP_KERNEL);
	if (arg == NULL)
		return -ENOMEM;

	vma->vm_private_data = arg;

	arg->task = current;
	arg->user_base = vma->vm_start;

	arg->kernel_base = (void *)phys_addr;
	return 0;
}

static int dpl_open(struct inode *inode, struct file *file)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (cpumask_weight(&(current->cpus_allowed)) != 1) {
		pr_err("Cannot open dpl device - " \
			"process must be binded to a specific cpu\n");
		return -EPERM;
	}

	if (smp_processor_id()) /* cpu 0 is not meant for dp processes */
		if (this_cpu_cmpxchg(dp_running_pid, 0, current->pid))
			/*
			 * locking the cpu from running other dp processes by
			 * setting the current pid to the dp_running_pid cpu
			 * variable. if the exchange returned a non-zero value,
			 * it means the exchange failed and another dp process
			 * is currently running on this cpu
			 */
			return -EBUSY;

	return 0;
}

static const struct file_operations dpl_fops = {
	.open		= dpl_open,
	.unlocked_ioctl	= dpl_ioctl,
	.mmap		= dpl_mmap,
};

static struct miscdevice dpl_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "dpl",
	.fops	= &dpl_fops
};

static int __init dpl_init(void)
{
	pr_info("Data Plane (DPL) driver for NPS device\n");
	return misc_register(&dpl_dev);
}
module_init(dpl_init);
