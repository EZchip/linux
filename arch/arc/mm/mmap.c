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
