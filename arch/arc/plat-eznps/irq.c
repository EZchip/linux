/*******************************************************************************

  Copyright(c) 2015 EZchip Technologies.

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

#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <asm/irq.h>
#include <asm/arcregs.h>
#include <plat/smp.h>

static void arch_mask_irq(struct irq_data *data)
{
	unsigned int ienb;

	ienb = read_aux_reg(AUX_IENABLE);
	ienb &= ~(1 << data->irq);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void arch_unmask_irq(struct irq_data *data)
{
	unsigned int ienb;

	ienb = read_aux_reg(AUX_IENABLE);
	ienb |= (1 << data->irq);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void arc_irq_mask(struct irq_data *data)
{
	arch_mask_irq(data);
}

static void arc_irq_unmask(struct irq_data *data)
{
	arch_unmask_irq(data);
}

static void arc_irq_ack(struct irq_data *data)
{
	write_aux_reg(CTOP_AUX_IACK, 1 << data->irq);
}

static struct irq_chip arc_intc_percpu = {
	.name		= "ARC Intc percpu",
	.irq_mask	= arc_irq_mask,
	.irq_unmask	= arc_irq_unmask,
	.irq_ack	= arc_irq_ack,
};

static struct irq_chip arc_intc = {
	.name		= "ARC Intc       ",
	.irq_mask	= arc_irq_mask,
	.irq_unmask	= arc_irq_unmask,
	.irq_eoi	= arc_irq_ack,
};

static int arc_intc_domain_map(struct irq_domain *d, unsigned int irq,
			       irq_hw_number_t hw)
{
	switch (irq) {
	case TIMER0_IRQ:
#ifdef CONFIG_SMP
	case IPI_IRQ:
#endif /* CONFIG_SMP */
		irq_set_chip_and_handler(irq,
			&arc_intc_percpu, handle_percpu_irq);
	break;
	default:
		irq_set_chip_and_handler(irq,
			&arc_intc, handle_fasteoi_irq);
	break;
	}

	return 0;
}

static const struct irq_domain_ops arc_intc_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = arc_intc_domain_map,
};

static struct irq_domain *root_domain;

static int __init
init_onchip_IRQ(struct device_node *intc, struct device_node *parent)
{
	if (parent)
		panic("DeviceTree incore intc not a root irq controller\n");

	root_domain = irq_domain_add_legacy(intc, NR_CPU_IRQS, 0, 0,
					    &arc_intc_domain_ops, NULL);

	if (!root_domain)
		panic("root irq domain not avail\n");

	/* with this we don't need to export root_domain */
	irq_set_default_host(root_domain);

	return 0;
}

IRQCHIP_DECLARE(arc_intc, "ezchip,arc700-intc", init_onchip_IRQ);
