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

struct dpl_arg {
	void __iomem *addr;
	void *buf;
	int len;
	int write;
};

struct dpl_mmap_arg {
	struct task_struct *task;
	resource_size_t user_base;
	void __iomem *kernel_base;
};

static void dpl_access_phys_local(void *arg)
{
	struct dpl_arg *a = (struct dpl_arg *)arg;

	if (arg == NULL)
		return;

	if (a->write)
		memcpy_toio(a->addr, a->buf, a->len);
	else
		memcpy_fromio(a->buf, a->addr, a->len);
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
	offset = addr - mmap_arg->user_base;
	arg.addr = mmap_arg->kernel_base + offset;
	cpu = task_cpu(mmap_arg->task);

	preempt_disable();
	smp_call_function_single(cpu, dpl_access_phys_local, &arg, 1);
	preempt_enable();

	return len;
}

static void dpl_vma_close(struct vm_area_struct * area)
{
	struct dpl_mmap_arg *mmap_arg = area->vm_private_data;

	iounmap(mmap_arg->kernel_base);
	kfree(mmap_arg);
}

static int dpl_minor = 1;

static const struct vm_operations_struct dpl_mem_ops = {
	.access = dpl_access_phys,
	.close = dpl_vma_close,
};

static int mmap_dpl(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	resource_size_t phys_addr = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long prot = vma->vm_page_prot;
	struct dpl_mmap_arg *arg;

	vma->vm_ops = &dpl_mem_ops;

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
	arg->user_base = vma->vm_start;

	/*
	 * map whole dpl in advanced
	 * It is not done in dpl_access_phys() since it is during IPI
	 * And we cannot get virtual memory area during interrupt context.
	 */
	arg->kernel_base = ioremap_prot(phys_addr, size, prot);

	return 0;
}

static const struct file_operations dpl_fops = {
	.mmap		= mmap_dpl,
};

static int dpl_open(struct inode *inode, struct file *filp)
{
	if (iminor(inode) != dpl_minor)
		return -ENXIO;

	filp->f_op = &dpl_fops;
	filp->f_mapping->backing_dev_info = &directly_mappable_cdev_bdi;
	filp->f_mode |= FMODE_UNSIGNED_OFFSET;

	return 0;
}

static const struct file_operations dev_fops = {
	.open = dpl_open,
};

static struct class *dpl_class;

static int __init chr_dev_init(void)
{
	int dpl_major;

	dpl_major = register_chrdev(0, "dpl", &dev_fops);
	if (dpl_major < 0) {
		printk("unable to get dynamic major for dpl dev\n");
		return dpl_major;
	}

	dpl_class = class_create(THIS_MODULE, "dpl");
	if (IS_ERR(dpl_class))
		return PTR_ERR(dpl_class);

	device_create(dpl_class, NULL, MKDEV(dpl_major, dpl_minor),
		      NULL, "dpl");

	return 0;
}

module_init(chr_dev_init);
