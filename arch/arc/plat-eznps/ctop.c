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

#include <linux/sched.h>
#include <asm/arcregs.h>
#include <plat/ctop.h>
#ifdef CONFIG_HAVE_HW_BREAKPOINT
#include <linux/hw_breakpoint.h>
#endif

void dp_save_restore(struct task_struct *prev, struct task_struct *next)
{
	struct eznps_dp *prev_task_dp = &prev->thread.dp;
	struct eznps_dp *next_task_dp = &next->thread.dp;

	/* Here we save all Data Plane related auxiliary registers */
	cpu_relax();
	prev_task_dp->dpc = read_aux_reg(CTOP_AUX_DPC);
	write_aux_reg(CTOP_AUX_DPC, next_task_dp->dpc);
	prev_task_dp->eflags = read_aux_reg(CTOP_AUX_EFLAGS);
	write_aux_reg(CTOP_AUX_EFLAGS, next_task_dp->eflags);
	prev_task_dp->gpa1 = read_aux_reg(CTOP_AUX_GPA1);
	write_aux_reg(CTOP_AUX_GPA1, next_task_dp->gpa1);
}

#ifdef CONFIG_HAVE_HW_BREAKPOINT
void plat_update_bp_info(struct perf_event *bp)
{
	int tid;
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	/* read thread id */
	tid = read_aux_reg(CTOP_AUX_THREAD_ID);

	/* update control fields*/
	info->ctrl.tid = tid;
	info->ctrl.tidmatch = 1;
}
#endif /* CONFIG_HAVE_HW_BREAKPOINT */

