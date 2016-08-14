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

#include <linux/sched.h>
#include <asm/arcregs.h>
#include <plat/ctop.h>
#include <plat/smp.h>
#ifdef CONFIG_HAVE_HW_BREAKPOINT
#include <linux/hw_breakpoint.h>
#endif

enum {
	post_write                                      = 0,
	non_post_write                                  = 1,
	post_wide_write                                 = 2,
	non_post_wide_write                             = 3,
	burst_write                                     = 4,
	burst_write_safe                                = 5,
	addr_base_maintenance                           = 7,
	single_read                                     = 8,
	burst_read                                      = 12,
	burst_l2_read                                   = 14,
};

enum {
	invalid_msid                                    = 0,
	addr_range_violation                            = 1,
	alignment_size                                  = 2,
	not_supported_transaction                       = 4,
	illegal_field_value                             = 5,
	write_privilege_violation                       = 6,
	uncorrectable_ecc                               = 15,
};

/*
 * Print memory information after a machine check exception, by
 * parsing the err_cap_1, err_cap_2 and err_sts registers
 */
int print_memory_exception(void)
{
	unsigned long long memory_offset;
	unsigned int msb_memory_offset, lsb_memory_offset;
	unsigned int cpu, ciu_block_id, err_cap_1_value, err_sts_value;
	unsigned int *err_cap_1_ptr, *err_cap_2_ptr, *err_sts_ptr;
	struct nps_ciu_err_cap_2 err_cap_2;

	/* getting the registers values */
	cpu = smp_processor_id();
	ciu_block_id = NPS_CPU_TO_CIU_ID(cpu);

	err_cap_1_ptr = (unsigned int *)
		nps_host_reg(cpu, ciu_block_id, NPS_CIU_ERR_CAP_1);
	err_cap_1_value = *err_cap_1_ptr;

	err_cap_2_ptr = (unsigned int *)
		nps_host_reg(cpu, ciu_block_id, NPS_CIU_ERR_CAP_2);
	err_cap_2.value = *err_cap_2_ptr;

	err_sts_ptr = (unsigned int *)
		nps_host_reg(cpu, ciu_block_id, NPS_CIU_ERR_STS);
	err_sts_value = *err_sts_ptr;

	/* returning 1 if the error is not caused by memory exception */
	if (err_sts_value == 0)
		return 1;

	/*
	 * the 35th and 34th bits of the memory offset are the first
	 * two bits of ERR_CAP_2, bits 33 to 3 are contained in
	 * ERR_CAP_1 and the first two bits will be zero.
	 * msb_memory_offset gets the 4 msbs of the 36 bit offset
	 * and lsb_memory_offset gets the rest of the bits
	 */
	msb_memory_offset = (err_cap_2.addr <<
		ERR_CAP_2_ADDRESS_SHIFT_LEFT_VALUE) | (err_cap_1_value >>
		ERR_CAP_1_SHIFT_RIGHT_VALUE);
	lsb_memory_offset = (err_cap_1_value << ERR_CAP_1_SHIFT_LEFT_VALUE);
	memory_offset = msb_memory_offset;
	memory_offset <<= 32;
	memory_offset |= lsb_memory_offset;

	pr_cont("Memory error exception");
	pr_err_with_indent("MSID = %d\n", err_cap_2.msid);
	pr_err_with_indent("Offset: 0x%llx\n", memory_offset);
	pr_err_with_indent("Error code = 0x%x => ", err_cap_2.erc);

	/* parsing the error code (reason of the exception) */
	switch (err_cap_2.erc) {
	case invalid_msid:
		pr_cont("Invalid MSID");
		break;
	case addr_range_violation:
		pr_cont("Address range violation");
		break;
	case alignment_size:
		pr_cont("Alignment/Size error");
		break;
	case not_supported_transaction:
		pr_cont("Not supported transaction");
		break;
	case illegal_field_value:
		pr_cont("Illegal field value");
		break;
	case write_privilege_violation:
		pr_cont("Write protection privilege violation");
		break;
	case uncorrectable_ecc:
		pr_cont("Uncorrectable ECC data error");
		break;
	default:
		pr_cont("Unknown error");
	}

	pr_err_with_indent("Transaction code = 0x%x => ", err_cap_2.rqtc);

	/* parsing the transaction code */
	switch (err_cap_2.rqtc) {
	case post_write:
		pr_cont("Posted write-single or atomic write-update");
		break;
	case non_post_write:
		pr_cont("Non-posted write-single or atomic write-update");
		break;
	case post_wide_write:
		pr_cont("Posted wide write-single or wide atomic write-update");
		break;
	case non_post_wide_write:
		pr_cont("Non-posted wide write-single or wide atomic ");
		pr_cont("write-update");
		break;
	case burst_write:
		pr_cont("Burst write");
		break;
	case burst_write_safe:
		pr_cont("Burst write-safe");
		break;
	case addr_base_maintenance:
		pr_cont("Address based maintenance commands");
		break;
	case single_read:
		pr_cont("Single read / atomic read-update");
		break;
	case burst_read:
		pr_cont("Burst read");
		break;
	case burst_l2_read:
		pr_cont("Burst read with L2 cache line");
		break;
	default:
		pr_cont("Unknown RQTC");
	}

	return 0;
}

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

