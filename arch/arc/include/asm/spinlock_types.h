#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

typedef struct {
	volatile unsigned int slock;
} raw_spinlock_t;

#define RAW_SPIN_LOCK_UNLOCKED	0
#define RAW_SPIN_LOCK_LOCKED	1

#define __RAW_SPIN_LOCK_UNLOCKED	{ RAW_SPIN_LOCK_UNLOCKED }

/*
   On ARC 800, the only atomic operation that is supported is exchange,
   instruction ex. We need atomic increment and decrement operations to
   implement the read write locks. The work around is to use a spinlock to
   get exclusive access to the read write lock
*/

typedef struct {
	raw_spinlock_t 			lock_mutex;
	volatile unsigned int 	lock;
} raw_rwlock_t;

#define __RAW_RW_LOCK_UNLOCKED		{ __RAW_SPIN_LOCK_UNLOCKED, RW_LOCK_BIAS }

#endif
