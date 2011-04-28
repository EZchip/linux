/*
 *  arch/arc/mm/mmap.c
 *
 *  Custom mmap layout for ARC700
 *
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 * Vineet Gupta <vineet.gupta@arc.com>
 *
 *  -mmap randomisation (borrowed from x86/ARM/s390 implementations)
 *  -TODO: Page Coloring for shared Code pages since they can potentially
 *    suffer from Cache Aliasing on VIPT ARC700 I-Cache
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/random.h>
#include <linux/module.h>
#include <asm/mman.h>
#include <linux/file.h>
#include <asm/uaccess.h>

/* common code for old and new mmaps */
static unsigned long do_mmap2(unsigned long addr_hint, unsigned long len,
                unsigned long prot, unsigned long flags,
                unsigned long fd, unsigned long pgoff)
{
    unsigned long vaddr = -EBADF;
    struct file *file = NULL;

    flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

    if (!(flags & MAP_ANONYMOUS)) {
        file = fget(fd);
        if (!file)
            goto out;
    }

    down_write(&current->mm->mmap_sem);

    vaddr = do_mmap_pgoff(file, addr_hint, len, prot, flags, pgoff);

    up_write(&current->mm->mmap_sem);

    if (file)
        fput(file);
out:
    return vaddr;
}

unsigned long sys_mmap2(unsigned long addr, unsigned long len,
              unsigned long prot, unsigned long flags,
              unsigned long fd, unsigned long pgoff)
{
    return do_mmap2(addr, len, prot, flags, fd, pgoff);
}

/*
 * Perform the select(nd, in, out, ex, tv) and mmap() system
 * calls. Linux/i386 didn't use to be able to handle more than
 * 4 system call parameters, so these system calls used a memory
 * block for parameter passing..
 */

struct mmap_arg_struct {
    unsigned long addr;
    unsigned long len;
    unsigned long prot;
    unsigned long flags;
    unsigned long fd;
    unsigned long offset;
};

asmlinkage int old_mmap(struct mmap_arg_struct *arg)
{
    struct mmap_arg_struct a;
    int err = -EFAULT;

    if (copy_from_user(&a, arg, sizeof(a)))
        goto out;

    err = -EINVAL;
    if (a.offset & ~PAGE_MASK)
        goto out;

    err =
        do_mmap2(a.addr, a.len, a.prot, a.flags, a.fd,
             a.offset >> PAGE_SHIFT);
      out:
    return err;
}


static inline int mmap_is_legacy(void)
{
#ifdef CONFIG_ARCH_ARC_SPACE_RND
    /* ELF loader sets this flag way early.
     * So no need to check for multiple things like
     *   !(current->personality & ADDR_NO_RANDOMIZE)
     *   randomize_va_space
     */
    return !(current->flags & PF_RANDOMIZE);
#else
    return 1;
#endif
}

static inline unsigned long arc_mmap_base(void)
{
    int rnd = 0;
    unsigned long base;

    /* 8 bits of randomness: 255 * 8k = 2MB */
    if (!mmap_is_legacy()) {
        rnd = (long)get_random_int() % (1 <<8);
        rnd <<= PAGE_SHIFT;
    }

    base = PAGE_ALIGN(TASK_UNMAPPED_BASE + rnd);

    //printk("RAND: mmap base orig %x rnd %x\n",TASK_UNMAPPED_BASE, base);

    return base;
}

void arch_pick_mmap_layout(struct mm_struct *mm)
{
    mm->mmap_base = arc_mmap_base();
	mm->get_unmapped_area = arch_get_unmapped_area;
	mm->unmap_area = arch_unmap_area;
}
EXPORT_SYMBOL_GPL(arch_pick_mmap_layout);
