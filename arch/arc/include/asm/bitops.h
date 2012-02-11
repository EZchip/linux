/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 * Vineetg: July 2009 (EXT2 bitops API optimisation)
 *	-Atomic API no longer call spin_lock as we are Uni-processor
 *	-Non Atomix API no longer disables interrupts
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H


#include <linux/compiler.h>
#include <linux/irqflags.h>

#ifdef CONFIG_SMP

#include <linux/spinlock_types.h>

extern spinlock_t    smp_bitops_lock;
extern unsigned long _spin_lock_irqsave(spinlock_t *lock);
extern void _spin_unlock_irqrestore(spinlock_t *lock, unsigned long);

#define bitops_lock(flags)   flags = _spin_lock_irqsave(&smp_bitops_lock)
#define bitops_unlock(flags) _spin_unlock_irqrestore(&smp_bitops_lock, flags)

#else


#define bitops_lock(flags)   local_irq_save(flags)
#define bitops_unlock(flags) local_irq_restore(flags)

#endif


#ifdef __KERNEL__

static inline void
set_bit(unsigned long nr, volatile void * addr)
{
    unsigned long temp, flags;
    int *m = ((int *) addr) + (nr >> 5);

    bitops_lock(flags);

    __asm__ __volatile__(
    "   ld%U3 %0,%3\n\t"
    "   bset %0,%0,%2\n\t"
    "   st%U1 %0,%1\n\t"
    :"=&r" (temp), "=o" (*m)
    :"Ir" (nr), "m" (*m));

    bitops_unlock(flags);
}

/*
 * WARNING: non atomic version.
 */
static inline void
__set_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bset %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));
}

// TODO does this affect uni-processor code

#define smp_mb__before_clear_bit()  barrier()
#define smp_mb__after_clear_bit()   barrier()

static inline void
clear_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp, flags;
  int *m = ((int *) addr) + (nr >> 5);

  bitops_lock(flags);

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bclr %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  bitops_unlock(flags);
}


static inline void
__clear_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bclr %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));
}

/*
 * WARNING: non atomic version.
 */
static inline void
__change_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bxor %0,%0,%2\n\t"
       "    st%U1 %0,%1"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));
}

static inline void
change_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp, flags;
  int *m = ((int *) addr) + (nr >> 5);

  bitops_lock(flags);

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bxor %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  bitops_unlock(flags);
}

static inline int
test_and_set_bit(unsigned long nr, volatile void *addr)
{
  unsigned long temp, flags;
  int *m = ((int *) addr) + (nr >> 5);
  // int old = *m; This is wrong, we are reading data before getting lock
  int old;

  bitops_lock(flags);

  old = *m;

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bset  %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  bitops_unlock(flags);

  return (old & (1 << (nr & 0x1f))) != 0;
}

/*
 * WARNING: non atomic version.
 */
static inline int
__test_and_set_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);
  int old = *m;
  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bset  %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  return (old & (1 << (nr & 0x1f))) != 0;
}

static inline int
test_and_clear_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp, flags;
  int *m = ((int *) addr) + (nr >> 5);
  int old;

  bitops_lock(flags);

  old = *m;

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bclr  %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  bitops_unlock(flags);

  return (old & (1 << (nr & 0x1f))) != 0;
}

/*
 * WARNING: non atomic version.
 */
static inline int
__test_and_clear_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);
  int old = *m;

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bclr  %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  return (old & (1 << (nr & 0x1f))) != 0;
}

/*
 * WARNING: non atomic version.
 */
static inline int
__test_and_change_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp;
  int *m = ((int *) addr) + (nr >> 5);
  int old = *m;

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bxor %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  return (old & (1 << (nr & 0x1f))) != 0;
}

static inline int
test_and_change_bit(unsigned long nr, volatile void * addr)
{
  unsigned long temp, flags;
  int *m = ((int *) addr) + (nr >> 5);
  int old;

  bitops_lock(flags);

  old = *m;

  __asm__ __volatile__(
       "    ld%U3 %0,%3\n\t"
       "    bxor %0,%0,%2\n\t"
       "    st%U1 %0,%1\n\t"
       :"=&r" (temp), "=o" (*m)
       :"Ir" (nr), "m" (*m));

  bitops_unlock(flags);

  return (old & (1 << (nr & 0x1f))) != 0;
}

/*
 * This routine doesn't need to be atomic.
 */
static inline int __constant_test_bit(int nr, const volatile void * addr)
{
       return ((1UL << (nr & 31)) & (((const volatile unsigned int *) addr)[nr >> 5])) != 0;
}

static inline int __test_bit(int nr, const volatile void * addr)
{
    int     * a = (int *) addr;
    int mask;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    return ((mask & *a) != 0);
}

#define test_bit(nr,addr) \
(__builtin_constant_p(nr) ? \
 __constant_test_bit((nr),(addr)) : \
 __test_bit((nr),(addr)))

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
static inline unsigned long ffz(unsigned long word)
{
    unsigned long result = 0;

    while(word & 1) {
        result++;
        word >>= 1;
    }
    return result;
}


#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)


#define ext2_set_bit(nr, addr)  \
    __test_and_set_bit((nr), (unsigned long *)(addr))
#define ext2_clear_bit(nr, addr) \
    __test_and_clear_bit((nr), (addr))

#define ext2_test_bit(nr, addr) \
    test_bit((nr), (unsigned long *)(addr))
#define ext2_find_first_zero_bit(addr, size) \
    find_first_zero_bit((unsigned long *)(addr), (size))
#define ext2_find_next_zero_bit(addr, size, offset) \
    find_next_zero_bit((unsigned long *)(addr), (size), (offset))
#define ext2_find_next_bit(addr, size, offset) \
    find_next_bit((unsigned long *)(addr), (size), (offset))

#define ext2_set_bit_atomic(lock, nr, addr) test_and_set_bit((nr), (addr))
#define ext2_clear_bit_atomic(lock, nr, addr) test_and_clear_bit((nr), (addr))

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr) set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

/**
 * hweightN - returns the hamming weight of a N-bit word
 * @x: the word to weigh
 *
 * The Hamming Weight of a number is the total number of bits set in it.
 */

#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/find.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/lock.h>
#endif /* __KERNEL__ */

#endif
