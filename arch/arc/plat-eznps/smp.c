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

#include <linux/smp.h>
#include <linux/of_fdt.h>
#include <linux/io.h>
#include <plat/smp.h>
#include <plat/mtm.h>

#define NPS_DEFAULT_MSID	0x34

static char smp_cpuinfo_buf[128];

/* Get cpu map from device tree */
static int __init eznps_get_map(const char *name, struct cpumask *cpumask)
{
	unsigned long dt_root = of_get_flat_dt_root();
	const char *buf;

	buf = of_get_flat_dt_prop(dt_root, name, NULL);
	if (!buf)
		return 1;

	cpulist_parse(buf, cpumask);

	return 0;
}

/* Update board cpu maps */
static void __init eznps_set_maps(void)
{
	struct cpumask cpumask;

	if (eznps_get_map("present-cpus", &cpumask)) {
		pr_err("Failed to get present-cpus from dtb");
		return;
	}
	init_cpu_present(&cpumask);

	if (eznps_get_map("possible-cpus", &cpumask)) {
		pr_err("Failed to get possible-cpus from dtb");
		return;
	}
	init_cpu_possible(&cpumask);
}

static void __init eznps_map_cpus(int max_cpus)
{
	if (max_cpus > 0)
		return;

	eznps_set_maps();
}

static void eznps_init_core(unsigned int cpu)
{
	u32 sync_value;
	struct nps_host_reg_aux_hw_comply hw_comply;
	struct nps_host_reg_aux_lpc lpc;

	if (NPS_CPU_TO_THREAD_NUM(cpu) != 0)
		return;

	write_aux_reg(CTOP_AUX_EFLAGS, 0);

	hw_comply.value = read_aux_reg(AUX_REG_HW_COMPLY);
	hw_comply.me  = 1;
	hw_comply.le  = 1;
	hw_comply.te  = 1;
	hw_comply.knc = 1;
	write_aux_reg(AUX_REG_HW_COMPLY, hw_comply.value);

	/* Enable MMU clock */
	lpc.mep = 1;
	write_aux_reg(CTOP_AUX_LPC, lpc.value);

	/* Boot CPU only */
	if (!cpu) {
		/* Write to general purpose register in CRG */
		sync_value = ioread32be(REG_GEN_PURP_0);
		sync_value |= NPS_CRG_SYNC_BIT;
		iowrite32be(sync_value, REG_GEN_PURP_0);
	}
}

/*
 * Any SMP specific init any CPU does when it comes up.
 * Here we setup the CPU to enable Inter-Processor-Interrupts
 * Called for each CPU
 * -Master      : init_IRQ()
 * -Other(s)    : start_kernel_secondary()
 */
void eznps_smp_init_cpu(unsigned int cpu)
{
	unsigned int rc;

	rc = smp_ipi_irq_setup(cpu, IPI_IRQ);
	if (rc)
		panic("IPI IRQ %d reg failed on BOOT cpu\n", IPI_IRQ);

	eznps_init_core(cpu);
	mtm_enable_core(cpu);
}

/*
 * Master kick starting another CPU
 */
static void eznps_smp_wakeup_cpu(int cpu, unsigned long pc)
{
	struct nps_host_reg_mtm_cpu_cfg cpu_cfg;

	if (mtm_enable_thread(cpu) == 0)
		return;

	/* check pc alignment */
	if (!IS_ALIGNED(pc, ARC_PLAT_START_PC_ALIGN))
		panic("pc is not properly aligned:%lx", pc);

	/* set PC, dmsid, and start CPU */
	cpu_cfg.value = pc;
	cpu_cfg.dmsid = NPS_DEFAULT_MSID;
	cpu_cfg.cs = 1;
	iowrite32be(cpu_cfg.value, nps_mtm_reg_addr(cpu, NPS_MTM_CPU_CFG));
}

static void eznps_ipi_send(int cpu)
{
	struct global_id gid;
	struct {
		union {
			struct {
				u32 num:8, cluster:8, core:8, thread:8;
			};
			u32 value;
		};
	} ipi;

	gid.value = cpu;
	ipi.thread = get_thread(gid);
	ipi.core = gid.core;
	ipi.cluster = nps_cluster_logic_to_phys(gid.cluster);
	ipi.num = IPI_IRQ;

	__asm__ __volatile__(
	"	mov r3, %0\n"
	"	.word %1\n"
	:
	: "r"(ipi.value), "i"(CTOP_INST_ASRI_0_R3)
	: "r3");
}

void __init eznps_init_early_smp(void)
{
	sprintf(smp_cpuinfo_buf, "Extn [EZNPS-SMP]\t: On\n");
	plat_smp_ops.info = smp_cpuinfo_buf;
	plat_smp_ops.ipi_send = eznps_ipi_send;
	plat_smp_ops.ipi_clear = NULL;
	plat_smp_ops.cpu_kick = eznps_smp_wakeup_cpu;
	plat_smp_ops.map_cpus = eznps_map_cpus;
}
