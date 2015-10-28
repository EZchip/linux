/*
 * Copyright(c) 2015 EZchip Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <soc/nps/common.h>

#define NPS_NR_CPU_IRQS 8  /* number of interrupt lines of NPS400 CPU */
#define NPS_TIMER0_IRQ  3

/*
 * NPS400 core includes an Interrupt Controller (IC) support.
 * All cores can deactivate level irqs at first level control
 * at cores mesh layer called MTM.
 * For devices out side chip e.g. uart, network there is another
 * level called Global Interrupt Manager (GIM).
 * This second level can control level and edge interrupt.
 *
 * NOTE: AUX_IENABLE and CTOP_AUX_IACK are auxiliary registers
 * with private HW copy per CPU.
 */

static void nps400_irq_mask(struct irq_data *irqd)
{
	unsigned int ienb;
	unsigned int irq = irqd_to_hwirq(irqd);

	ienb = read_aux_reg(AUX_IENABLE);
	ienb &= ~(1 << irq);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void nps400_irq_unmask(struct irq_data *irqd)
{
	unsigned int ienb;
	unsigned int irq = irqd_to_hwirq(irqd);

	ienb = read_aux_reg(AUX_IENABLE);
	ienb |= (1 << irq);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void nps400_irq_eoi_global(struct irq_data *irqd)
{
	unsigned int __maybe_unused irq = irqd_to_hwirq(irqd);

	write_aux_reg(CTOP_AUX_IACK, 1 << irq);

	/* Don't ack GIC before all device access attempts are done */
	mb();

	nps_ack_gic();
}

static void nps400_irq_eoi(struct irq_data *irqd)
{
	unsigned int __maybe_unused irq = irqd_to_hwirq(irqd);

	write_aux_reg(CTOP_AUX_IACK, 1 << irq);
}

static struct irq_chip nps400_irq_chip_fasteoi = {
	.name		= "NPS400 IC Global",
	.irq_mask	= nps400_irq_mask,
	.irq_unmask	= nps400_irq_unmask,
	.irq_eoi	= nps400_irq_eoi_global,
};

static struct irq_chip nps400_irq_chip_percpu = {
	.name		= "NPS400 IC",
	.irq_mask	= nps400_irq_mask,
	.irq_unmask	= nps400_irq_unmask,
	.irq_eoi	= nps400_irq_eoi,
};

static int nps400_irq_map(struct irq_domain *d, unsigned int virq,
			  irq_hw_number_t hw)
{
	switch (hw) {
	case NPS_TIMER0_IRQ:
#ifdef CONFIG_SMP
	case IPI_IRQ:
#endif
		irq_set_percpu_devid(virq);
		irq_set_chip_and_handler(virq, &nps400_irq_chip_percpu,
					 handle_percpu_devid_irq);
		break;
	default:
		irq_set_chip_and_handler(virq, &nps400_irq_chip_fasteoi,
					 handle_fasteoi_irq);
		break;
	}

	return 0;
}

static const struct irq_domain_ops nps400_irq_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = nps400_irq_map,
};

static int __init nps400_of_init(struct device_node *node,
				 struct device_node *parent)
{
	static struct irq_domain *nps400_root_domain;

	if (parent) {
		pr_err("DeviceTree incore ic not a root irq controller\n");
		return -EINVAL;
	}

	nps400_root_domain = irq_domain_add_linear(node, NPS_NR_CPU_IRQS,
						   &nps400_irq_ops, NULL);

	if (!nps400_root_domain) {
		pr_err("nps400 root irq domain not avail\n");
		return -ENOMEM;
	}

	/*
	 * Needed for primary domain lookup to succeed
	 * This is a primary irqchip, and can never have a parent
	 */
	irq_set_default_host(nps400_root_domain);

#ifdef CONFIG_SMP
	irq_create_mapping(nps400_root_domain, IPI_IRQ);
#endif

	return 0;
}
IRQCHIP_DECLARE(ezchip_nps400_ic, "ezchip,nps400-ic", nps400_of_init);
