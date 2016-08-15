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

enum {
	private_imem					= 0,
	stack_imem					= 1,
	half_cluster_data				= 2,
	half_cluster_code				= 3,
	x1_cluster_data					= 4,
	x1_cluster_code					= 5,
	x2_cluster_data					= 6,
	x2_cluster_code					= 7,
	x4_cluster_data					= 8,
	x4_cluster_code					= 9,
	x16_cluster_data				= 10,
	x16_cluster_code				= 11,
	all_cluster_data				= 12,
	all_cluster_code				= 13,
	global_io_all_cluster_code			= 14,
	emem						= 15,
};

static void check_en_or_wp(unsigned int en, unsigned int wp)
{
	if (!en)
		pr_err_with_indent("This memory space is not enabled\n");

	if (!wp) {
		pr_err_with_indent("Write to this memory space is not ");
		pr_cont("allowed\n");
	}
}

/*
 * Parse the type of the memory space by the msid, which should be lower than
 * 16
 */
static void print_memory_space_type(unsigned int msid)
{
	switch (msid) {
	case private_imem:
		pr_cont("private ");
		break;
	case stack_imem:
		pr_cont("stack ");
		break;
	case half_cluster_data:
		pr_cont("half cluster data ");
		break;
	case half_cluster_code:
		pr_cont("half cluster code ");
		break;
	case x1_cluster_data:
		pr_cont("x1 cluster data ");
		break;
	case x1_cluster_code:
		pr_cont("x1 cluster code ");
		break;
	case x2_cluster_data:
		pr_cont("x2 cluster data ");
		break;
	case x2_cluster_code:
		pr_cont("x2 cluster code ");
		break;
	case x4_cluster_data:
		pr_cont("x4 cluster data ");
		break;
	case x4_cluster_code:
		pr_cont("x4 cluster code ");
		break;
	case x16_cluster_data:
		pr_cont("x16 cluster data ");
		break;
	case x16_cluster_code:
		pr_cont("x16 cluster code ");
		break;

	/*
	 * those memory spaces designed in case the chip
	 * should have a bigger number of clusters in
	 * the future
	*/
	case all_cluster_data:
		pr_cont("all cluster data ");
		break;
	case all_cluster_code:
		pr_cont("all cluster code ");
		break;

	/*
	 * memory space containing for the buffers of the
	 * crossbars in the sides of the chip, also for all
	 * the clusters
	*/
	case global_io_all_cluster_code:
		pr_cont("global IO all cluster code ");
		break;

	case emem:
		pr_cont("emem\n");
	}

	if (msid < emem)
		pr_cont("imem\n");
}

/*
 * Print memory information after a machine check exception, by
 * parsing the err_cap_1, err_cap_2, err_sts and various other
 * registers found in the CIU or MTM
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
	pr_err_fmt_with_indent("MSID = %d => ", err_cap_2.msid);

	if (err_cap_2.emem_bits)
		pr_cont("EMEM memory space with msid index %d\n",
			err_cap_2.msid);
	else if (err_cap_2.msid > emem)
		pr_cont("default msid\n");
	else {
		int relevant_block_id;
		unsigned int mtm_block_id = NPS_CPU_TO_MTM_ID(cpu);
		struct nps_mtm_msid_cfg0_1 *ms0_1 =
			(struct nps_mtm_msid_cfg0_1 *) (nps_host_reg(cpu,
				mtm_block_id,
				NPS_MTM_MSID_CFG_BASE + err_cap_2.msid));

		struct nps_mtm_msid_cfg2_5 *ms2_5 =
			(struct nps_mtm_msid_cfg2_5 *) (nps_host_reg(cpu,
			mtm_block_id,
			NPS_MTM_MSID_CFG_BASE + err_cap_2.msid));

		struct nps_ciu_fmt_msid_cfg *ms6_14 =
			(struct nps_ciu_fmt_msid_cfg *) (nps_host_reg(cpu,
			ciu_block_id, NPS_CIU_FMT_MSID_CFG_BASE -
				x2_cluster_data + err_cap_2.msid));

		struct nps_ciu_fmt_msid_cfg_15 *ms15 =
			(struct nps_ciu_fmt_msid_cfg_15 *) (nps_host_reg(cpu,
			ciu_block_id, NPS_CIU_FMT_MSID_CFG_BASE -
				x2_cluster_data + err_cap_2.msid));

		/* getting the relevant block id */
		if (err_cap_2.msid < x2_cluster_data)
			relevant_block_id = mtm_block_id;
		else
			relevant_block_id = ciu_block_id;

		print_memory_space_type(err_cap_2.msid);

		/*
		 * checking if the memory space is enabled or if the
		 * write is permitted
		*/
		if (err_cap_2.msid < half_cluster_data)
			check_en_or_wp(ms0_1->en, ms0_1->wp);
		else if (err_cap_2.msid < x2_cluster_data)
			check_en_or_wp(ms2_5->en, ms2_5->wp);
		else if (err_cap_2.msid < emem)
			check_en_or_wp(ms6_14->en, ms6_14->wp);
		else
			check_en_or_wp(ms15->en, ms15->wp);
	}

	pr_err_fmt_with_indent("Offset: 0x%llx\n", memory_offset);
	pr_err_fmt_with_indent("Error code = 0x%x => ", err_cap_2.erc);

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

	pr_err_fmt_with_indent("Transaction code = 0x%x => ", err_cap_2.rqtc);

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

/*
 * Provide information about the sizes of the cmem private section,
 * shared section and data cache section, and also about the access
 * (whether it was on private section, etc...)
 */
static int provide_cmem_sizes_information(unsigned long address,
		struct nps_host_reg_aux_udmc udmc)
{
	struct nps_host_reg_aux_cmpc cmpc;
	unsigned int private_cmem_size;
	unsigned int shared_cmem_size;
	unsigned int data_cache_size;
	unsigned int active_threads;
	unsigned int private_cmem_offset = 0;
	unsigned int shared_cmem_offset = 0;

	cmpc.value = read_aux_reg(CTOP_AUX_CMPC);

	pr_info("invalid CMEM access\n");
	pr_info("virtual address: 0x%lx\n", address);

	/* parsing the CMEM information for the user */
	if (address < CMEM_SHARED_BASE) {
		/* in private area */
		private_cmem_offset = address - CMEM_BASE;
		pr_info("private CMEM, offset: 0x%x", private_cmem_offset);
	} else {
		/* in shared area */
		shared_cmem_offset = address - CMEM_SHARED_BASE;
		pr_info("shared CMEM, offset: 0x%x", shared_cmem_offset);
	}

	private_cmem_size = GET_PRIVATE_CMEM_SIZE(cmpc);
	pr_info("private CMEM start address: 0x%x\n", CMEM_BASE);
	pr_info("private CMEM size: 0x%x bytes\n", private_cmem_size);
	pr_info("maximum allowed private CMEM access: 0x%x (offset of 0x%x)\n",
		CMEM_BASE + private_cmem_size, private_cmem_size);

	/* parsing the size of the data cache */
	if (udmc.dcas == 0)
		data_cache_size = 0;
	else if (udmc.dcas <= CTOP_AUX_UDMC_DCAS_MAX_VALUE)
		data_cache_size = GET_DATA_CACHE_SIZE(udmc);
	else {
		pr_err("invalid dcas field in udmc register: %d\n", udmc.dcas);

		/* sending SIGSEGV */
		return 1;
	}

	/* parsing the number of active threads */
	active_threads = GET_ACTIVE_THREADS(udmc);
	if (udmc.nat > CTOP_AUX_UDMC_NAT_MAX_VALUE) {
		pr_err("invalid nat field in udmc register: %d\n", udmc.nat);

		/* sending SIGSEGV */
		return 1;
	}

	shared_cmem_size = GET_SHARED_CMEM_SIZE(private_cmem_size,
				data_cache_size,
				active_threads);

	pr_info("shared CMEM start address: 0x%x\n", CMEM_SHARED_BASE);
	pr_info("shared CMEM size: 0x%x bytes\n", shared_cmem_size);
	pr_info("maximum allowed shared CMEM access: 0x%x (offset of 0x%x)\n",
			CMEM_SHARED_BASE + shared_cmem_size, shared_cmem_size);
	pr_info("data cache size: 0x%x bytes\n", data_cache_size);

	if (!shared_cmem_offset &&
		private_cmem_offset >= private_cmem_size)
		pr_info("offset exceeding private cmem size\n");
	else if (!private_cmem_offset &&
		shared_cmem_offset >= shared_cmem_size)
		pr_info("offset exceeding shared cmem size\n");

	/* sending SIGSEGV */
	return 1;
}

/*
 * Provide information about the fmt slot
 */
static int provide_fmt_slot_information(unsigned long address)
{
	struct nps_host_reg_fmtp fmtp;

	/* each slot is 4 bytes long */
	unsigned int slot_offset = GET_FMT_AUX_REG_ADDR(address);

	fmtp.value = read_aux_reg(CTOP_FMTP_BASE + slot_offset);

	/*
	 * if FMT isn't valid, then we treat this as a
	 * regular tlb miss
	 */
	if (fmtp.v) {
		pr_info("invalid memory access\n");
		pr_info("virtual address: 0x%lx\n", address);
		pr_info("MSID = %d => ", fmtp.msid);
		print_memory_space_type(fmtp.msid);
		pr_info("offset: 0x%lx\n", address & GENMASK(FMT_SHIFT - 1, 0));

		/*
		 * we assume that the fmt slot has write privilege, otherwise
		 * there's no reason a tlb miss exception would be generated
		 * when the fmt slot is valid
		 */
		pr_info("write from this fmt slot is not allowed\n");

		/* sending SIGSEGV */
		return 1;
	}

	/* nothing to be done */
	return 0;
}

/*
 * Provide information about a tlb miss and returns and integer
 * indicating in what way to handle this exception from fault.c
 */
int provide_nps_mapping_information(unsigned long address)
{
	struct nps_host_reg_aux_dpc dpc;

	dpc.value = read_aux_reg(CTOP_AUX_DPC);
	if (!dpc.men)
		/*
		 * dp application memory transactions are not available, so
		 * no relevant information may be parsed
		 */
		return 0;

	if (address < FMT_START) { /* CMEM */
		struct nps_host_reg_aux_udmc udmc;

		udmc.value = read_aux_reg(CTOP_AUX_UDMC);
		/*
		 * if CMEM isn't valid, then we treat this as a
		 * regular tlb miss
		 */
		if (udmc.cme)
			return provide_cmem_sizes_information(address, udmc);
	} else /* FMT SLOTS */
		return provide_fmt_slot_information(address);

	/* nothing to be done */
	return 0;
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

