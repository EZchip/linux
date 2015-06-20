/*
 * HW_breakpoint: a unified kernel/user-space hardware breakpoint facility,
 * using the CPU's debug registers.
 */
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/percpu.h>
#include <linux/context_tracking.h> /* exception_enter(), ... */
#include <asm/arcregs.h>
#include <asm/hw_breakpoint.h>

/*
 * Stores the breakpoints currently in use on each breakpoint address
 * register for each cpus
 */
static DEFINE_PER_CPU(struct perf_event *, bp_per_reg[ARC_MAX_HBP_SLOTS]);

/* Number of usable hardware breakpoints registers on this CPU. */
static int core_num_hbps;

/*
 * Update breakpoint perf_event before writing to registers.
 */
void __weak plat_update_bp_info(struct perf_event *bp) {}

/*
 * Install a perf counter breakpoint.
 */
int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	int i, offset;
	u32 addr;

	addr = info->address;

	for (i = 0; i < core_num_hbps; i++) {
		if (!__this_cpu_read(bp_per_reg[i])) {
			__this_cpu_write(bp_per_reg[i], bp);
			break;
		}
	}

	if (WARN_ONCE(i == core_num_hbps, "Can't find any breakpoint slot"))
		return -EBUSY;

	/* debug register offset */
	offset = i*3;

	/* Set thread id and enable match. */
	plat_update_bp_info(bp);

	/* Enable the breakpoint. */
	info->ctrl.type = info->type;

	/* Setup the address in the AMV register. */
	write_aux_reg(ARC_BASE_AMV + offset, addr);
	/* Setup the control in the AC register. */
	write_aux_reg(ARC_BASE_AC + offset, info->ctrl.value);

	return 0;
}

void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	int i, offset;

	/* Remove the breakpoint. */
	for (i = 0; i < core_num_hbps; i++) {
		if (__this_cpu_read(bp_per_reg[i]) == bp) {
			__this_cpu_write(bp_per_reg[i], NULL);
			break;
		}
	}

	if (WARN_ONCE(i == core_num_hbps, "Can't find any breakpoint slot"))
		return;

	/* debug register offset */
	offset = i*3;
	/* Setup the control in the AC register. */
	write_aux_reg(ARC_BASE_AC + offset, 0);
}

static int get_hbp_len(u8 hbp_len)
{
	unsigned int len_in_bytes = 0;

	switch (hbp_len) {
	case ARC_BREAKPOINT_LEN_1:
		len_in_bytes = 1;
		break;
	case ARC_BREAKPOINT_LEN_2:
		len_in_bytes = 2;
		break;
	case ARC_BREAKPOINT_LEN_4:
		len_in_bytes = 4;
		break;
	case ARC_BREAKPOINT_LEN_8:
		len_in_bytes = 8;
		break;
	}

	return len_in_bytes;
}

/*
 * Check whether bp virtual address is in kernel space.
 */
int arch_check_bp_in_kernelspace(struct perf_event *bp)
{
	unsigned int len;
	unsigned long va;
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	va = info->address;
	len = get_hbp_len(info->ctrl.len);

	return (va >= TASK_SIZE) && ((va + len - 1) >= TASK_SIZE);
}

/*
 * Extract generic type and length encodings from an arch_hw_breakpoint_ctrl.
 * Hopefully this will disappear when ptrace can bypass the conversion
 * to generic breakpoint descriptions.
 */
int arch_bp_generic_fields(struct arch_hw_breakpoint_ctrl ctrl,
			   int *gen_len, int *gen_type)
{
	/* Type */
	switch (ctrl.type) {
	case ARC_BREAKPOINT_EXECUTE:
		*gen_type = HW_BREAKPOINT_X;
		break;
	case ARC_BREAKPOINT_READ:
		*gen_type = HW_BREAKPOINT_R;
		break;
	case ARC_BREAKPOINT_WRITE:
		*gen_type = HW_BREAKPOINT_W;
		break;
	case ARC_BREAKPOINT_RW:
		*gen_type = HW_BREAKPOINT_RW;
		break;
	default:
		return -EINVAL;
	}

	/* Len */
	switch (ctrl.len) {
	case 0:
		*gen_type = HW_BREAKPOINT_EMPTY;
		*gen_len = 0;
		break;
	case ARC_BREAKPOINT_LEN_1:
		*gen_len = HW_BREAKPOINT_LEN_1;
		break;
	case ARC_BREAKPOINT_LEN_2:
		*gen_len = HW_BREAKPOINT_LEN_2;
		break;
	case ARC_BREAKPOINT_LEN_4:
		*gen_len = HW_BREAKPOINT_LEN_4;
		break;
	case ARC_BREAKPOINT_LEN_8:
		*gen_len = HW_BREAKPOINT_LEN_8;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Construct an arch_hw_breakpoint from a perf_event.
 */
static int arch_build_bp_info(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	/* Type */
	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_R:
		info->type = ARC_BREAKPOINT_READ;
		break;
	case HW_BREAKPOINT_W:
		info->type = ARC_BREAKPOINT_WRITE;
		break;
	case HW_BREAKPOINT_RW:
		info->type = ARC_BREAKPOINT_RW;
		break;
	case HW_BREAKPOINT_X:
		info->type = ARC_BREAKPOINT_READ;
		break;
	default:
		return -EINVAL;
	}

	/* Len */
	switch (bp->attr.bp_len) {
	case HW_BREAKPOINT_LEN_1:
		info->ctrl.len = ARC_BREAKPOINT_LEN_1;
		break;
	case HW_BREAKPOINT_LEN_2:
		info->ctrl.len = ARC_BREAKPOINT_LEN_2;
		break;
	case HW_BREAKPOINT_LEN_4:
		info->ctrl.len = ARC_BREAKPOINT_LEN_4;
		break;
	case HW_BREAKPOINT_LEN_8:
		info->ctrl.len = ARC_BREAKPOINT_LEN_8;
		break;
	default:
		return -EINVAL;
	}

	/* Address */
	info->address = bp->attr.bp_addr;

	/* Action */
	info->ctrl.action = ARC_BREAKPOINT_ACTION_SOFT_EXP;

	/* Target */
	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_R:
	case HW_BREAKPOINT_W:
	case HW_BREAKPOINT_RW:
		info->ctrl.target = ARC_BREAKPOINT_TARGET_LDST_ADDR;
		break;
	case HW_BREAKPOINT_X:
		info->ctrl.target = ARC_BREAKPOINT_TARGET_INST_ADDR;
		break;
	default:
		return -EINVAL;
	}

	/* Enabled? */
	if (bp->attr.disabled)
		info->ctrl.type = ARC_BREAKPOINT_DISABLED;
	else
		info->ctrl.type = info->type;

	return 0;
}

/*
 * Validate the arch-specific HW Breakpoint register settings.
 */
int arch_validate_hwbkpt_settings(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	unsigned int align;
	int ret;

	ret = arch_build_bp_info(bp);
	if (ret)
		return ret;

	ret = -EINVAL;

	switch (info->ctrl.len) {
	case ARC_BREAKPOINT_LEN_1:
		align = 0;
		break;
	case ARC_BREAKPOINT_LEN_2:
		align = 1;
		break;
	case ARC_BREAKPOINT_LEN_4:
		align = 3;
		break;
	default:
		return ret;
	}

	/*
	 * Check that the low-order bits of the address are appropriate
	 * for the alignment implied by len.
	 */
	if (info->address & align)
		return -EINVAL;

	return 0;
}

/* Determine number of usable hardware breakpoints available. */
static int get_num_hbps(void)
{
	struct {
		union {
			struct {
#ifdef CONFIG_CPU_BIG_ENDIAN
				u32 __reserved:20, param:4, unused:8;
#else
				u32 unused:8, param:4, __reserved:20;
#endif
			};
			u32 value;
		};
	} ap_build;

	ap_build.value = read_aux_reg(ARC_AP_BUILD);

	if (!ap_build.value)
		return 0;

	switch (ap_build.param) {
	case 0:
		return 2;
	case 1:
		return 4;
	case 2:
		return 8;
	case 4:
		return 2;
	case 5:
		return 4;
	case 6:
		return 8;
	default:
		pr_warn("Invalid number of usable hardware breakpoints\n");
		return 0;
	}
}

static int __init arch_hw_breakpoint_init(void)
{
	/* Determine how many HBPs are available. */
	core_num_hbps = get_num_hbps();

	pr_info("Found %d usable hardware breakpoints registers.\n",
			core_num_hbps);

	return 0;
}
arch_initcall(arch_hw_breakpoint_init);

int hw_breakpoint_slots(int type)
{
	return get_num_hbps();
}

void hw_breakpoint_pmu_read(struct perf_event *bp) {}

/*
 * Dummy function to register with die_notifier.
 */
int hw_breakpoint_exceptions_notify(struct notifier_block *unused,
					unsigned long val, void *data)
{
	return NOTIFY_DONE;
}

static struct perf_event *do_debugreg_hit(unsigned int dr_num,
		struct pt_regs *regs)
{
	struct perf_event *bp;
	struct arch_hw_breakpoint *info;
	unsigned int offset = dr_num*3;

	bp = __this_cpu_read(bp_per_reg[dr_num]);
	if (bp == NULL)
		return NULL;

	info = counter_arch_bp(bp);

	/* Read the address from the AMV register. */
	info->trigger = read_aux_reg(ARC_BASE_AMV + offset);

	perf_bp_event(bp, regs);

	return bp;
}

/*
 * Entry point for action point hit
 */
void do_actionpoint_hit(unsigned long address, struct pt_regs *regs)
{
	unsigned int param = regs->ecr_param;
	struct perf_event *bp;
	unsigned int i;
	enum ctx_state prev_state;

	prev_state = exception_enter();

	rcu_read_lock();
	preempt_disable();

	for (i = 0; i < ARC_MAX_HBP_SLOTS; i++) {
		/* Check if the i-th actionpoint triggered */
		if (!((param >> i) & 0x1))
			continue;

		bp = do_debugreg_hit(i, regs);

		if (bp == NULL)
			goto unlock;
	}

unlock:
	preempt_enable();
	rcu_read_unlock();

	exception_exit(prev_state);
}
