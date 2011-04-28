/*
 *  linux/include/asm-arc/ide.h
 *
 *  ARC IDE Interface
 *
 *  Copyright (C) 1994-2005  Linus Torvalds & authors
 *  ARC IDE functions - ARC International, 2005-2007
 *
 */

#ifndef __ASMARC_IDE_H
#define __ASMARC_IDE_H

#ifdef __KERNEL__

#ifdef MAX_HWIFS
#undef MAX_HWIFS
#endif
#define MAX_HWIFS   1

/* We need the hook to handle the unexpected interrupts and to handle the
 * interrupts in PIO Mode. The interrupt handler for PIO Mode is task_in_intr
 * from generic IDE code which doesn't ack at the Controller
 */
#define IDE_ARCH_ACK_INTR
#define ide_ack_intr(hwif)	((hwif)->ack_intr ? (hwif)->ack_intr(hwif) : 1)

/*
 * We always use the new IDE port registering,
 * so these are fixed here.
 */

#define ide_default_io_base(i)  (0)
#define ide_default_irq(b)      (IDE_IRQ)

#include <asm-generic/ide_iops.h>


#endif /* __KERNEL__ */

#endif
