/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spinlock.h>

DEFINE_SPINLOCK(smp_bitops_lock);

#ifdef CONFIG_SMP
#define bitops_lock(flags)		spin_lock_irqsave(&smp_bitops_lock, flags)
#define bitops_unlock(flags)	spin_unlock_irqrestore(&smp_bitops_lock, flags)
#else
#define bitops_lock(flags)   local_irq_save(flags)
#define bitops_unlock(flags) local_irq_restore(flags)
#endif

void set_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long temp, flags;
    m += nr >> 5;

    bitops_lock(flags);

    __asm__ __volatile__(
    "   ld%U3 %0,%3\n\t"
    "   bset %0,%0,%2\n\t"
    "   st%U1 %0,%1\n\t"
    :"=&r" (temp), "=o" (*m)
    :"ir" (nr), "m" (*m));

    bitops_unlock(flags);
}

void clear_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long temp, flags;
    m += nr >> 5;

    bitops_lock(flags);

    __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bclr %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"ir" (nr), "m" (*m));

    bitops_unlock(flags);
}

void change_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long temp, flags;
    m += nr >> 5;

    bitops_lock(flags);

    __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bxor %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"ir" (nr), "m" (*m));

    bitops_unlock(flags);
}


int test_and_set_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long old, temp, flags;
    m += nr >> 5;
    // int old = *m; This is wrong, we are reading data before getting lock

    bitops_lock(flags);

    old = *m;

    __asm__ __volatile__(
       "    bset  %0,%3,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"ir" (nr), "r"(old));

    bitops_unlock(flags);

    if (__builtin_constant_p(nr)) nr &= 0x1f;

    return (old & (1 << nr)) != 0;
}


int test_and_clear_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long temp, old, flags;
    m += nr >> 5;

    bitops_lock(flags);

    old = *m;

    __asm__ __volatile__(
       "    bclr  %0,%3,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"ir" (nr), "r"(old));

    bitops_unlock(flags);

    if (__builtin_constant_p(nr)) nr &= 0x1f;

    return (old & (1 << nr)) != 0;
}


int test_and_change_bit(unsigned long nr, volatile unsigned long *m)
{
    unsigned long temp, old, flags;
    m += nr >> 5;

    bitops_lock(flags);

    old = *m;

    __asm__ __volatile__(
       "    bxor %0,%3,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"ir" (nr), "r"(old));

    bitops_unlock(flags);

    if (__builtin_constant_p(nr)) nr &= 0x1f;

    return (old & (1 << nr)) != 0;
}
