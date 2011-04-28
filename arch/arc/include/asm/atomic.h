/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 *  linux/include/asm-arc/atomic.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef _ASM_ARC_ATOMIC_H
#define _ASM_ARC_ATOMIC_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/irqflags.h>

#define ATOMIC_INIT(i)  { (i) }


/*
    On ARC, the only atomic operation provided is exchange operation. To provide
    the software workaround for other atomic operations (inc, dec, add ....) we
    disable interrupts in the UP and use spin lock in the MP.

    In case of UP, the spin lock api saves the flags and disables the interrupts
    so that all the hardware resuources are available to the current process

    In case of MP, spin lock api saves the flags, disables the interrupts and
    acquires the global lock (spin lock) which is atomic operation, and then
    modify the atomic_t variable - (during which) the process may be context
    switched, but the access to the atomic variable is still exclusively held
    the process.
*/


#ifdef CONFIG_SMP

#include <linux/spinlock_types.h>

extern spinlock_t smp_atomic_ops_lock;
extern unsigned long    _spin_lock_irqsave(spinlock_t *lock);
extern void _spin_unlock_irqrestore(spinlock_t *lock, unsigned long);


// TODO-vineetg need to cleanup and user Linux exported APIs instead of 2nd level ones
#define atomic_ops_lock(flags)   flags = _spin_lock_irqsave(&smp_atomic_ops_lock)
#define atomic_ops_unlock(flags) _spin_unlock_irqrestore(&smp_atomic_ops_lock, flags)

#else

#define atomic_ops_lock(flags)      local_irq_save(flags)
#define atomic_ops_unlock(flags)    local_irq_restore(flags)

#endif


#ifdef __KERNEL__

/* FIXME :: not atomic on load store architecture ?
 * i think it wont matter. */
#define atomic_read(v)  ((v)->counter)

#ifdef CONFIG_SMP

static __inline__ void atomic_set(volatile atomic_t *v, int i)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter = i;
    atomic_ops_unlock(flags);
}

#else

#define atomic_set(v,i) (((v)->counter) = (i))

#endif


static __inline__ void atomic_add(int i, volatile atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter += i;
    atomic_ops_unlock(flags);
}

static __inline__ void atomic_sub(int i, volatile atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter -= i;
    atomic_ops_unlock(flags);
}

static __inline__ int atomic_add_return(int i, volatile atomic_t *v)
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

static __inline__ int atomic_sub_return(int i, volatile atomic_t *v)
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

static __inline__ void atomic_inc(volatile atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter += 1;
    atomic_ops_unlock(flags);
}

static __inline__ void atomic_dec(volatile atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter -= 1;
    atomic_ops_unlock(flags);
}

static __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
    unsigned long flags;
    int result;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp -= 1;
    result = (temp == 0);
    v->counter = temp;
    atomic_ops_unlock(flags);

    return result;
}

static __inline__ int atomic_add_negative(int i, volatile atomic_t *v)
{
    unsigned long flags;
    int result;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp += i;
    result = (temp < 0);
    v->counter = temp;
    atomic_ops_unlock(flags);

    return result;
}

static __inline__ void atomic_clear_mask(unsigned long mask, unsigned long *addr)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    *addr &= ~mask;
    atomic_ops_unlock(flags);
}

#define atomic_cmpxchg(v, o, n) ((int)cmpxchg(&((v)->counter), (o), (n)))

static __inline__ unsigned long
cmpxchg(volatile int *p, int old, int new)
{
    unsigned long flags;
    int prev;

    atomic_ops_lock(flags);
    if ((prev = *p) == old)
        *p = new;
    atomic_ops_unlock(flags);
    return(prev);
}

#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

/**
 * atomic_add_unless - add unless the number is a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as it was not @u.
 * Returns non-zero if @v was not @u, and zero otherwise.
 */
#define atomic_add_unless(v, a, u)              \
({                              \
    int c, old;                     \
    c = atomic_read(v);                 \
    while (c != (u) && (old = atomic_cmpxchg((v), c, c + (a))) != c) \
        c = old;                    \
    c != (u);                       \
})

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

#define atomic_inc_and_test(v)  (atomic_add_return(1, v) == 0)
#define atomic_dec_return(v)    atomic_sub_return(1, (v))
#define atomic_inc_return(v)    atomic_add_return(1, (v))
#define atomic_sub_and_test(i,v)  (atomic_sub_return(i, v) == 0)

#define smp_mb__before_atomic_dec() barrier()
#define smp_mb__after_atomic_dec()  barrier()
#define smp_mb__before_atomic_inc() barrier()
#define smp_mb__after_atomic_inc()  barrier()

#include <asm-generic/atomic-long.h>
#endif
#endif
