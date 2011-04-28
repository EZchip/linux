/*************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * vineetg: Feb 2009
 *  -For AA4 board, IRQ assignments to peripherals
 *
 ************************************************************************/

#ifndef __PLAT_IRQ_H
#define __PLAT_IRQ_H

#ifdef CONFIG_SMP
#define NR_IRQS 32
#else
#define NR_IRQS 16
#endif

#define TIMER0_INT      3
#define TIMER1_INT      4

#define VUART_IRQ       5
#define VUART1_IRQ      10
#define VUART2_IRQ      11

#define VMAC_IRQ        6

#define IDE_IRQ         13
#define PCI_IRQ         14

#ifdef CONFIG_SMP
#define IDU_INTERRUPT_0 16
#endif

#endif
