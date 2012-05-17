/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spinlock.h>

DEFINE_SPINLOCK(smp_atomic_ops_lock);

#ifdef CONFIG_SMP
#define atomic_ops_lock(flags)		spin_lock_irqsave(&smp_atomic_ops_lock, flags)
#define atomic_ops_unlock(flags)	spin_unlock_irqrestore(&smp_atomic_ops_lock, flags)
void atomic_set(atomic_t *v, int i)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter = i;
    atomic_ops_unlock(flags);
}
#else
#define atomic_ops_lock(flags)      local_irq_save(flags)
#define atomic_ops_unlock(flags)    local_irq_restore(flags)
#endif

void atomic_add(int i, atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter += i;
    atomic_ops_unlock(flags);
}

void atomic_sub(int i, atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter -= i;
    atomic_ops_unlock(flags);
}

int atomic_add_return(int i, atomic_t *v)
{
    unsigned long flags ;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp += i;
    v->counter = temp;
    atomic_ops_unlock(flags);

    return temp;
}

int atomic_sub_return(int i, atomic_t *v)
{
    unsigned long flags;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp -= i;
    v->counter = temp;
    atomic_ops_unlock(flags);

    return temp;
}

void atomic_clear_mask(unsigned long mask, unsigned long *addr)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    *addr &= ~mask;
    atomic_ops_unlock(flags);
}

/* unlike other APIS, cmpxchg is same as atomix_cmpxchg because
 * because the sematics of cmpxchg itself is to be atomic
 */
unsigned long __cmpxchg(volatile void *ptr, unsigned long expected, unsigned long new)
{
    unsigned long flags;
    int prev;
    volatile unsigned long *p = ptr;

    atomic_ops_lock(flags);
    if ((prev = *p) == expected)
        *p = new;
    atomic_ops_unlock(flags);
    return(prev);
}
