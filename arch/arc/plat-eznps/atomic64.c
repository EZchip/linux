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

#include <linux/types.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <plat/perf_event.h>

/*
 * We use a hashed array of spinlocks to provide exclusive access
 * to each atomic64_t variable.  Since this is expected to used on
 * systems with small numbers of CPUs (<= 4 or so), we use a
 * relatively small array of 16 spinlocks to avoid wasting too much
 * memory on the spinlock array.
 */
#define NR_LOCKS	16

/*
 * Ensure each lock is in a separate cacheline.
 */
static union {
	raw_spinlock_t lock;
	char pad[L1_CACHE_BYTES];
} atomic64_lock[NR_LOCKS] __cacheline_aligned_in_smp = {
	[0 ... (NR_LOCKS - 1)] = {
		.lock =  __RAW_SPIN_LOCK_UNLOCKED(atomic64_lock.lock),
	},
};

static inline raw_spinlock_t *lock_addr(const atomic64_t *v)
{
	unsigned long addr = (unsigned long) v;

	addr >>= L1_CACHE_SHIFT;
	addr ^= (addr >> 8) ^ (addr >> 16);
	return &atomic64_lock[addr & (NR_LOCKS - 1)].lock;
}

long long atomic64_read(const atomic64_t *v)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);
	long long val;

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	val = v->counter;
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
	return val;
}
EXPORT_SYMBOL(atomic64_read);

void atomic64_set(atomic64_t *v, long long i)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	v->counter = i;
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
}
EXPORT_SYMBOL(atomic64_set);

#define ATOMIC64_OP(op, c_op)						\
void atomic64_##op(long long a, atomic64_t *v)				\
{									\
	unsigned long flags;						\
	unsigned long pflags;						\
	raw_spinlock_t *lock = lock_addr(v);				\
									\
	perf_control_save(pflags);					\
	raw_spin_lock_irqsave(lock, flags);				\
	v->counter c_op a;						\
	raw_spin_unlock_irqrestore(lock, flags);			\
	perf_control_restore(pflags);					\
}									\
EXPORT_SYMBOL(atomic64_##op);

#define ATOMIC64_OP_RETURN(op, c_op)					\
long long atomic64_##op##_return(long long a, atomic64_t *v)		\
{									\
	unsigned long flags;						\
	unsigned long pflags;						\
	raw_spinlock_t *lock = lock_addr(v);				\
	long long val;							\
									\
	perf_control_save(pflags);					\
	raw_spin_lock_irqsave(lock, flags);				\
	val = (v->counter c_op a);					\
	raw_spin_unlock_irqrestore(lock, flags);			\
	perf_control_restore(pflags);					\
	return val;							\
}									\
EXPORT_SYMBOL(atomic64_##op##_return);

#define ATOMIC64_OPS(op, c_op)						\
	ATOMIC64_OP(op, c_op)						\
	ATOMIC64_OP_RETURN(op, c_op)

ATOMIC64_OPS(add, +=)
ATOMIC64_OPS(sub, -=)
ATOMIC64_OP(and, &=)
ATOMIC64_OP(or, |=)
ATOMIC64_OP(xor, ^=)

#undef ATOMIC64_OPS
#undef ATOMIC64_OP_RETURN
#undef ATOMIC64_OP

long long atomic64_dec_if_positive(atomic64_t *v)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);
	long long val;

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	val = v->counter - 1;
	if (val >= 0)
		v->counter = val;
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
	return val;
}
EXPORT_SYMBOL(atomic64_dec_if_positive);

long long atomic64_cmpxchg(atomic64_t *v, long long o, long long n)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);
	long long val;

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	val = v->counter;
	if (val == o)
		v->counter = n;
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
	return val;
}
EXPORT_SYMBOL(atomic64_cmpxchg);

long long atomic64_xchg(atomic64_t *v, long long new)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);
	long long val;

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	val = v->counter;
	v->counter = new;
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
	return val;
}
EXPORT_SYMBOL(atomic64_xchg);

int atomic64_add_unless(atomic64_t *v, long long a, long long u)
{
	unsigned long flags;
	unsigned long pflags;
	raw_spinlock_t *lock = lock_addr(v);
	int ret = 0;

	perf_control_save(pflags);
	raw_spin_lock_irqsave(lock, flags);
	if (v->counter != u) {
		v->counter += a;
		ret = 1;
	}
	raw_spin_unlock_irqrestore(lock, flags);
	perf_control_restore(pflags);
	return ret;
}
EXPORT_SYMBOL(atomic64_add_unless);
