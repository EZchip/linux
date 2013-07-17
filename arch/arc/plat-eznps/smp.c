/*******************************************************************************

  ARC700 Extensions for SMP
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

#include <linux/smp.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <plat/smp.h>
#include <asm/setup.h>

static char smp_cpuinfo_buf[128];

/*
 *-------------------------------------------------------------------
 * Platform specific callbacks expected by arch SMP code
 *-------------------------------------------------------------------
 */

const char *arc_platform_smp_cpuinfo(void)
{
	sprintf(smp_cpuinfo_buf, "Extn [700-SMP]\t: On\n");

	return smp_cpuinfo_buf;
}

/*
 * After power-up, a non Master CPU needs to wait for Master to kick start it
 *
 * W/o hardware assist, it will busy-spin on a token which is eventually set
 * by Master, preferably from arc_platform_smp_wakeup_cpu(). Once token is
 * available it needs to jump to @first_lines_of_secondary (using inline asm).
 *
 * The XTL H/w module, however allows Master to directly set Other CPU's PC as
 * well as ability to start/stop them. This allows implementing this function
 * as a simple dead stop using "FLAG 1" insn.
 * As a hack for debugging (debugger will single-step over the FLAG insn), we
 * anyways wrap it in a self loop
 *
 * Alert: can NOT use stack here as it has not been determined/setup for CPU.
 *        If it turns out to be elaborate, it's better to code it in assembly
 */
void arc_platform_smp_wait_to_boot(int cpu)
{
	/* Secondary Halts self. Later master will set PC and clear halt bit */
	__asm__ __volatile__(
	"1:\n"
	"	flag 1\n"
	"	b 1b\n");
}

/*
 * Master kick starting another CPU
 */
void arc_platform_smp_wakeup_cpu(int cpu, unsigned long pc)
{
	extern unsigned long eznps_he_version;
	if (eznps_he_version == 1)
	{
		unsigned long cpu_cfg_value = 0;
		unsigned int *cpu_cfg_reg = REG_CPU_CONFIG(cpu);

		/* check pc alignment */
		if (pc & NPS_CPU_MEM_ADDRESS_MASK)
			panic("pc is not properly aligned:%lx", pc);

		/* set pc */
		cpu_cfg_value = pc;

		/* set dmsid */
		cpu_cfg_value |= NPS_DEFAULT_MSID << NPS_DEFAULT_MSID_SHIFT;

		/* cpu start */
		cpu_cfg_value |= 1;

		__raw_writel(cpu_cfg_value, cpu_cfg_reg);
	}
	else
	{
		unsigned int halt_ctrl;

		/* override pc */
		pc = (unsigned long)ez_first_lines_of_secondary;

		/* setup the start PC */
		__raw_writel(pc, CPU_SEC_ENTRY_POINT);

		/* Take the cpu out of Halt */
		if (!running_on_hw) {
			halt_ctrl = 0;
		}
		else {
			halt_ctrl = __raw_readl(REG_CPU_HALT_CTL);
			halt_ctrl |= 1 << cpu;
		}
		__raw_writel(halt_ctrl, REG_CPU_HALT_CTL);
	}
}

/* Initialize Number of Active Threads */
static void init_nat(int cpu)
{
	char *__cpu_active_map  = (char*)(0xc0005000);
	int core = (cpu >> 4) & 0xff;
	int nat = __cpu_active_map[core];
	int log_nat = 0;
	long tmp;

	switch (nat) {
	case 1:
		log_nat = 0;
		break;
	case 2:
		log_nat = 1;
		break;
	case 4:
		log_nat = 2;
		break;
	case 8:
		log_nat = 3;
		break;
	case 16:
		log_nat = 4;
		break;
	default:
		break;
	}

	tmp = read_aux_reg(0xfffffb00);
	tmp &= 0xfffff8ff;
	tmp |= log_nat << 8;
	write_aux_reg(0xfffffb00,tmp);
}

/*
 * Any SMP specific init any CPU does when it comes up.
 * Here we setup the CPU to enable Inter-Processor-Interrupts
 * Called for each CPU
 * -Master      : init_IRQ()
 * -Other(s)    : start_kernel_secondary()
 */
void arc_platform_smp_init_cpu(void)
{
	extern unsigned long eznps_he_version;
	int cpu = smp_processor_id();
	int irq __maybe_unused;
	unsigned int tmp;

	if (eznps_he_version == 1 || !running_on_hw)
	{
		/* using edge */
		tmp = read_aux_reg(AUX_ITRIGGER);
		write_aux_reg(AUX_ITRIGGER,tmp | (1 << IPI_IRQ));

		if (cpu == 0) {
			int rc;

			rc = smp_ipi_irq_setup(cpu, IPI_IRQ);
			if (rc)
				panic("IPI IRQ %d reg failed on BOOT cpu\n",
					IPI_IRQ);
		}
		enable_percpu_irq(IPI_IRQ, 0);
		init_nat(cpu);
	}
	else
	{
		/* Attach the arch-common IPI ISR to our IPI IRQ */
		for (irq = 0; irq < num_possible_cpus(); irq++) {
			/* using edge */
			tmp = read_aux_reg(AUX_ITRIGGER);
			write_aux_reg(AUX_ITRIGGER,
					tmp | (1 << (IPI_IRQS_BASE + irq)));

			if (cpu == 0) {
				int rc;

				rc = smp_ipi_irq_setup(cpu, IPI_IRQS_BASE + irq);
				if (rc)
					panic("IPI IRQ %d reg failed on BOOT cpu\n",
						 irq);
			}
			enable_percpu_irq(IPI_IRQS_BASE + irq, 0);
		}
	}
}

void arc_platform_ipi_send(const struct cpumask *callmap)
{
	extern unsigned long eznps_he_version;
	if (eznps_he_version == 1 || !running_on_hw)
	{
		unsigned int cpu, reg_value = 0;
		for_each_cpu(cpu, callmap) {
			reg_value  = (NPS_CPU_TO_THREAD_NUM(cpu));
			reg_value |= (NPS_CPU_TO_CORE_NUM(cpu) << 8);
			reg_value |= (NPS_CPU_TO_CLUSTER(cpu) << 16);
			reg_value |= (IPI_IRQ << 24);

			__asm__ __volatile__(
					"mov r3, %0\n"
					".word 0x3b56003e #ipi 0, r3\n"
					:
					: "r"(reg_value)	
			);
		}
	}
	else
	{
		unsigned int cpu, this_cpu = smp_processor_id();

		for_each_cpu(cpu, callmap) {
			__raw_writel((1 << cpu), REGS_CPU_IPI(this_cpu));
			__raw_writel(0, REGS_CPU_IPI(this_cpu));
		}
	}
}

void arc_platform_ipi_clear(int cpu, int irq)
{
	write_aux_reg(AUX_IPULSE, (1 << irq));
}

