/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_IRQFLAGS_H
#define __ASM_ARC_IRQFLAGS_H

/* vineetg: March 2010 : local_irq_save( ) optimisation
 *  -Remove explicit mov of current status32 into reg, that is not needed
 *  -Use BIC  insn instead of INVERTED + AND
 *  -Conditionally disable interrupts (if they are not enabled, don't disable)
*/

#ifdef __KERNEL__

#include <asm/arcregs.h>

/******************************************************************
 * IRQ Control Macros
 ******************************************************************/

/*
 * Save IRQ state and disable IRQs
 */
static inline long arch_local_irq_save(void) {
    unsigned long temp, flags;

    __asm__ __volatile__ (
        "lr  %1, [status32]\n\t"
        "bic %0, %1, %2\n\t"    // a BIC b = a AND ~b, (now 4 byte vs. 8)
        "and.f 0, %1, %2  \n\t"
        "flag.nz %0\n\t"
        :"=r" (temp), "=r" (flags)
        :"n" ((STATUS_E1_MASK | STATUS_E2_MASK))
        :"cc"
    );

    return flags;
}

/*
 * restore saved IRQ state
 */
static inline void arch_local_irq_restore(unsigned long flags) {

    __asm__ __volatile__ (
        "flag %0\n\t"
        :
        :"r" (flags)
    );
}

/*
 * Conditionally Enable IRQs
 */
extern void arch_local_irq_enable(void);

/*
 * Unconditionally Disable IRQs
 */
static inline void arch_local_irq_disable(void) {
    unsigned long temp;

    __asm__ __volatile__ (
        "lr  %0, [status32]\n\t"
        "and %0, %0, %1\n\t" // {AND a,a,u7} is already (BIC not req)
        "flag %0\n\t"
        :"=&r" (temp)
        :"n" (~(STATUS_E1_MASK | STATUS_E2_MASK))
    );
}

/*
 * save IRQ state
 */
static inline long arch_local_save_flags(void) {
    unsigned long temp;

    __asm__ __volatile__ (
        "lr  %0, [status32]\n\t"
        :"=&r" (temp)
    );

    return temp;
}

/*
 * mask/unmask an interrupt (@x = IRQ bitmap)
 * e.g. to Disable IRQ 3 and 4, pass 0x18
 *
 * mask = disable IRQ = CLEAR bit in AUX_I_ENABLE
 * unmask = enable IRQ = SET bit in AUX_I_ENABLE
 */

#define mask_interrupt(x)  __asm__ __volatile__ (   \
    "lr r20, [auxienable] \n\t"                     \
    "and    r20, r20, %0 \n\t"                      \
    "sr     r20,[auxienable] \n\t"                  \
    :                                               \
    :"r" (~(x))                                     \
    :"r20", "memory")

#define unmask_interrupt(x)  __asm__ __volatile__ ( \
    "lr r20, [auxienable] \n\t"                     \
    "or     r20, r20, %0 \n\t"                      \
    "sr     r20, [auxienable] \n\t"                 \
    :                                               \
    :"r" (x)                                        \
    :"r20", "memory")

/*
 * Query IRQ state
 */
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
    return (!(flags & (STATUS_E1_MASK
#ifdef CONFIG_ARCH_ARC_LV2_INTR
                        | STATUS_E2_MASK
#endif
            )));
}

static inline int arch_irqs_disabled(void)
{
    unsigned long flags;
    flags = arch_local_save_flags();
    return (!(flags & (STATUS_E1_MASK
#ifdef CONFIG_ARCH_ARC_LV2_INTR
                        | STATUS_E2_MASK
#endif
            )));
}

#endif

#endif
