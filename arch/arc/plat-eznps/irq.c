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

#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <asm/irq.h>
#include <asm/arcregs.h>
#include <plat/smp.h>
#include <plat/mtm.h>

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

static void arc_irq_eoi(struct irq_data *data)
{
	write_aux_reg(CTOP_AUX_IACK, 1 << data->irq);

	__asm__ __volatile__ (
	"	nop\n	.word %0\n"
	"	.word %1\n"
	:
	: "i"(CTOP_INST_SCHD_RW), "i"(CTOP_INST_RSPI_GIC_0_R12)
	: "memory");
}

static struct irq_chip arc_intc_timer_percpu = {
        .name           = "ARC Intc percpu",
        .irq_mask       = arc_irq_mask,
        .irq_unmask     = arc_irq_unmask,
        .irq_eoi        = arc_irq_ack,
};

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
	.irq_eoi	= arc_irq_eoi,
};

static int arc_intc_domain_map(struct irq_domain *d, unsigned int irq,
			       irq_hw_number_t hw)
{
	switch (irq) {
	case TIMER0_IRQ:
		irq_set_chip_and_handler(irq,
			&arc_intc_timer_percpu, handle_percpu_irq);
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

static void __init eznps_configure_msu(void)
{
	int cpu;
	struct nps_host_reg_msu_en_cfg msu_en_cfg = {.value = 0};

	msu_en_cfg.msu_en = 1;
	msu_en_cfg.ipi_en = 1;
	msu_en_cfg.gim_0_en = 1;
	msu_en_cfg.gim_1_en = 1;

	/* enable IPI and GIM messages on all clusters */
	for (cpu = 0 ; cpu < eznps_max_cpus; cpu += eznps_cpus_per_cluster)
		iowrite32be(msu_en_cfg.value,
			nps_msu_reg_addr(cpu, NPS_MSU_EN_CFG));
}

static void __init eznps_configure_gim(void)
{
	u32 reg_value;
	u32 gim_int_lines;
	struct nps_host_reg_gim_p_int_dst gim_p_int_dst = {.value = 0};

	/* adding the low lines to the interrupt lines to set polarity */
	gim_int_lines = NPS_GIM_UART_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_EAST_RX_RDY_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_WEST_RX_RDY_LINE;

	/*
	* IRQ polarity
	* low or high level
	* negative or positive edge
	*/
	reg_value = ioread32be(REG_GIM_P_INT_POL_0);
	reg_value &= ~gim_int_lines;
	iowrite32be(reg_value, REG_GIM_P_INT_POL_0);

	/* IRQ type level or edge */
	reg_value = ioread32be(REG_GIM_P_INT_SENS_0);
	reg_value |= NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE;
	reg_value |= NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE;
	iowrite32be(reg_value, REG_GIM_P_INT_SENS_0);

	/*
	* GIM interrupt select type for
	* dbg_lan TX and RX interrupts
	* should be type 1
	* type 0 = IRQ line 6
	* type 1 = IRQ line 7
	*/
	gim_p_int_dst.is = 1;
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_10);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_11);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_25);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_26);

	/* adding watchdog interrupt to the interrupt lines */
	gim_int_lines |= NPS_GIM_WDOG_LINE;

	/*
	* CTOP IRQ lines should be defined
	* as blocking in GIM
	*/
	iowrite32be(gim_int_lines, REG_GIM_P_INT_BLK_0);

	/* watchdog interrupt should be sent to the interrupt out line */
	gim_p_int_dst.value = 0;
	gim_p_int_dst.int_out_en = 1;
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_0);
	iowrite32be(1, REG_GIM_IO_INT_EN);

	/* enable CTOP IRQ lines in GIM */
	iowrite32be(gim_int_lines, REG_GIM_P_INT_EN_0);
}

void __init eznps_plat_irq_init(void)
{
	eznps_configure_msu();

	eznps_configure_gim();
}

IRQCHIP_DECLARE(arc_intc, "ezchip,arc700-intc", init_onchip_IRQ);
