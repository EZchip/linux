/*******************************************************************************

  EZNPS Platform IRQ hookups
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#include <linux/interrupt.h>
#include <asm/irq.h>
#include <plat/memmap.h>
#include <asm/setup.h>

static void arc_ack_irq(struct irq_data *data)
{
	if (data->irq == IPI_IRQ)
		write_aux_reg(0xfffff888, 1 << data->irq);
}

static void arc_mask_irq(struct irq_data *data)
{
	arch_mask_irq(data->irq);
}

static void arc_unmask_irq(struct irq_data *data)
{
	arch_unmask_irq(data->irq);
}

static void arc_mask_ack_irq(struct irq_data *data)
{
	arch_mask_irq(data->irq);
	arc_ack_irq(data);
}

static unsigned int arc_startup_irq(struct irq_data *data)
{
#ifdef CONFIG_SMP
	if (data->irq == UART0_IRQ)
		smp_send_ienable(data->irq, true);
	else
		arch_unmask_irq(data->irq);
#else
	arch_unmask_irq(data->irq);
#endif

	return 0;
}

static void arc_shutdown_irq(struct irq_data *data)
{
#ifdef CONFIG_SMP
	if (data->irq == UART0_IRQ)
		smp_send_ienable(data->irq, false);
	else
		arch_mask_irq(data->irq);
#else
	arch_mask_irq(data->irq);
#endif
}

/*
 * There's no off-chip Interrupt Controller in the FPGA builds
 * Below sufficies a simple model for the on-chip controller, with
 * all interrupts being level triggered.
 */
static struct irq_chip eznps_chip = {
	.irq_mask	= arc_mask_irq,
	.irq_unmask	= arc_unmask_irq,
};

void __init plat_init_IRQ()
{
#ifdef CONFIG_SMP
	extern unsigned long eznps_he_version;
#endif
	int i;

	for (i = 0; i < NR_IRQS; i++)
		irq_set_chip_and_handler(i, &eznps_chip, handle_level_irq);

#ifdef CONFIG_SMP
	irq_set_chip_and_handler(TIMER0_INT, &eznps_chip, handle_percpu_irq);


	if (eznps_he_version == 1 || !running_on_hw)
	{
		eznps_chip.irq_mask_ack = arc_mask_ack_irq;
		eznps_chip.irq_startup = arc_startup_irq;
		eznps_chip.irq_shutdown = arc_shutdown_irq;
		eznps_chip.irq_ack = arc_ack_irq;
		irq_set_percpu_devid(IPI_IRQ);
		irq_set_chip_and_handler(IPI_IRQ, &eznps_chip, handle_percpu_irq);
	}
	else
	{
		for (i = IPI_IRQS_BASE; i < IPI_IRQS_BASE + num_possible_cpus(); i++) {
			irq_set_percpu_devid(i);
			irq_set_chip_and_handler(i, &eznps_chip, handle_percpu_irq);
		}
	}
#endif /* CONFIG_SMP */

	/*
	 * SMP Hack because UART IRQ hardwired to cpu0 (boot-cpu) but if the
	 * request_irq() comes from any other CPU, the low level IRQ unamsking
	 * essential for getting Interrupts won't be enabled on cpu0, locking
	 * up the UART state machine.
	 */
#ifdef CONFIG_SMP
	arch_unmask_irq(UART0_IRQ);
#endif
}
