/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/rwlock.h>
#include <asm/spinlock_types.h>
#include <asm/processor.h>

/*
 * SMP spinlocks, allowing only a single CPU anywhere
 * (the type definitions are in asm/spinlock_types.h)
 */


#define arch_spin_is_locked(x)		((x)->slock == ARCH_SPIN_LOCK_LOCKED)

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned int tmp = ARCH_SPIN_LOCK_LOCKED;

	__asm__ __volatile__(
						"1: ex	%0, [%1]\n\t"
						   "cmp %0, %2\n\t"
						   "beq 1b\n\t"
						:
						:"r"(tmp), "m"(lock->slock),"i"(ARCH_SPIN_LOCK_LOCKED)
						);
	smp_mb();
}


/*
 * It is easier for the lock validator if interrupts are not re-enabled
 * in the middle of a lock-acquire. This is a performance feature anyway
 * so we turn it off:
 */

#define arch_spin_lock_flags(lock, flags)	arch_spin_lock(lock)


static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int tmp = ARCH_SPIN_LOCK_LOCKED;

	__asm__ __volatile__("ex	%0, [%1]"
						:"=r"(tmp)
						:"m"(lock->slock),"0"(tmp)
						);

	if (tmp == ARCH_SPIN_LOCK_UNLOCKED) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_mb();
	__asm__ __volatile__("nop_s \n\t"
                         "st	%1, [%0]"
						: /* No output */
						:"r"(&(lock->slock)), "i"(ARCH_SPIN_LOCK_UNLOCKED)
						);
}

static inline void arch_spin_unlock_wait(arch_spinlock_t *lock)
{
	while (arch_spin_is_locked(lock))
		cpu_relax();
}




/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 */

/* read_can_lock - would read_trylock() succeed? */
#define __raw_read_can_lock(x)		((x)->lock > 0)

/* write_can_lock - would write_trylock() succeed? */
#define __raw_write_can_lock(x)		((x)->lock == RW_LOCK_BIAS)

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	arch_spin_lock (&(rw->lock_mutex));
	if (rw->lock > 0) {
		rw->lock--;
		arch_spin_unlock (&(rw->lock_mutex));
		return 1;
	}
	arch_spin_unlock (&(rw->lock_mutex));

	return 0;
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	arch_spin_lock (&(rw->lock_mutex));
	if (rw->lock == RW_LOCK_BIAS) {
		rw->lock = 0;
		arch_spin_unlock (&(rw->lock_mutex));
		return 1;
	}
	arch_spin_unlock (&(rw->lock_mutex));

	return 0;
}

static inline void arch_read_lock(arch_rwlock_t *rw)
{
	while(!arch_read_trylock(rw))
		cpu_relax();
}

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	while(!arch_write_trylock(rw))
		cpu_relax();
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	arch_spin_lock (&(rw->lock_mutex));
	rw->lock++;
	arch_spin_unlock (&(rw->lock_mutex));
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	arch_spin_lock (&(rw->lock_mutex));
	rw->lock = RW_LOCK_BIAS;
	arch_spin_unlock (&(rw->lock_mutex));
}

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#endif /* __ASM_SPINLOCK_H */
