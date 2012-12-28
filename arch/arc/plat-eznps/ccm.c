/*******************************************************************************

  EZNPS Platform support code
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/slab.h>

struct ccm_arg {
	unsigned long addr;
	void *buf;
	int len;
	int write;
};

struct ccm_mmap_arg {
	struct task_struct *task;
	void __iomem *addr;
};

static void ccm_access_phys_local(void *arg)
{
	struct ccm_arg *a;
	int offset;

	if (arg == NULL)
		return;

	a = (struct ccm_arg *)arg;
	offset = a->addr & (PAGE_SIZE-1);

	if (a->write)
		memcpy_toio(a->addr + offset, a->buf, a->len);
	else
		memcpy_fromio(a->buf, a->addr + offset, a->len);
}

static int ccm_access_phys(struct vm_area_struct *vma, unsigned long addr,
		void *buf, int len, int write)
{
	struct ccm_mmap_arg *mmap_arg;
	int cpu;
	struct ccm_arg arg = {
			.addr = addr,
			.buf = buf,
			.len = len,
			.write = write,
	};

	if (vma == NULL)
		return -EINVAL;

	if (vma->vm_private_data == NULL)
		return -EINVAL;

	mmap_arg = vma->vm_private_data;
	cpu = task_cpu(mmap_arg->task);

	preempt_disable();
	smp_call_function_single(cpu, ccm_access_phys_local, &arg, 1);
	preempt_enable();

	return len;
}

static int ccm_minor = 1;

static const struct vm_operations_struct ccm_mem_ops = {
	.access = ccm_access_phys
};

static int mmap_ccm(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	struct ccm_mmap_arg *arg;

	vma->vm_ops = &ccm_mem_ops;

	arg = kmalloc(sizeof(*arg), GFP_KERNEL);
	if (arg == NULL)
			return -ENOMEM;

	vma->vm_private_data = arg;

	/* Remap-pfn-range will mark the range VM_IO and VM_RESERVED */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    vma->vm_pgoff,
			    size,
			    vma->vm_page_prot)) {
		return -EAGAIN;
	}

	arg->task = current;

	/*
	 * We take one page for this mission in advance
	 * It is not done with generic_access_phys() since it is during IPI
	 * And we cannot get virtual memory area during interrupt context.
	 */
	arg->addr = ioremap_prot(vma->vm_pgoff, PAGE_SIZE, vma->vm_page_prot);

	return 0;
}

static const struct file_operations ccm_fops = {
	.mmap		= mmap_ccm,
};

static int ccm_open(struct inode *inode, struct file *filp)
{
	if (iminor(inode) != ccm_minor)
		return -ENXIO;

	filp->f_op = &ccm_fops;
	filp->f_mapping->backing_dev_info = &directly_mappable_cdev_bdi;
	filp->f_mode |= FMODE_UNSIGNED_OFFSET;

	return 0;
}

static const struct file_operations dev_fops = {
	.open = ccm_open,
};

static struct class *ccm_class;

static int __init chr_dev_init(void)
{
	int ccm_major;

	ccm_major = register_chrdev(0, "ccm", &dev_fops);
	if (ccm_major < 0) {
		printk("unable to get dynamic major for ccm dev\n");
		return ccm_major;
	}

	ccm_class = class_create(THIS_MODULE, "ccm");
	if (IS_ERR(ccm_class))
		return PTR_ERR(ccm_class);

	device_create(ccm_class, NULL, MKDEV(ccm_major, ccm_minor),
		      NULL, "ccm");

	return 0;
}

module_init(chr_dev_init);
