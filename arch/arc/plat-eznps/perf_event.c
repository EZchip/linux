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

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/perf_event.h>
#include <plat/perf_event.h>
#include <plat/ctop.h>
#include <linux/platform_device.h>
#include <asm/arcregs.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>


struct cpu_hw_events {
	/* Array of events on this cpu. */
	struct perf_event	*events[NPS_MAX_COUNTERS];

	/*
	 * Set the bit (indexed by the counter number) when the counter
	 * is used for an event.
	 */
	unsigned long		used_mask[BITS_TO_LONGS(NPS_MAX_COUNTERS)];
};
DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events);

/* NPS platform performance monitor unit */
struct nps_pmu {;
	int		counter_size;		/* in bits */
	int		n_counters;		/* total available counters */
	const int	*hw_events;		/* hw events table */
	const int	(*cache_events)[PERF_COUNT_HW_CACHE_MAX]
				       [PERF_COUNT_HW_CACHE_OP_MAX]
				       [PERF_COUNT_HW_CACHE_RESULT_MAX];
	int		(*map_hw_event)(u64);	/* method used to map
						   hw events */
	int		(*map_cache_event)(u64);/* method used to
						   map cache events */
	int		max_events;		/* max generic hw events
						   in map */
	u64		max_period;		/* max sampling period */
	u64		overflow;		/* counter width */
};

#define EVENT_OP_UNSUPPORTED -1

static const int nps_hw_event_map[] = {
	[PERF_COUNT_HW_CPU_CYCLES] =			76,	/* crun */
	[PERF_COUNT_HW_INSTRUCTIONS] =			2,	/* iall */
	[PERF_COUNT_HW_CACHE_REFERENCES] =		EVENT_OP_UNSUPPORTED,
	[PERF_COUNT_HW_CACHE_MISSES] =			EVENT_OP_UNSUPPORTED,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] =		13,	/* ijmp */
	[PERF_COUNT_HW_BRANCH_MISSES] =			105,	/* bpfail */
	[PERF_COUNT_HW_BUS_CYCLES] =			76,	/* crun */
	[PERF_COUNT_HW_STALLED_CYCLES_FRONTEND] =	106,	/* bflush */
	[PERF_COUNT_HW_STALLED_CYCLES_BACKEND] =	107,	/* bstall */
	[PERF_COUNT_HW_REF_CPU_CYCLES] =		76,	/* crun */
};

#define C(_x)			PERF_COUNT_HW_CACHE_##_x
#define CACHE_OP_UNSUPPORTED	0xffff

/*
 * Generalized hw caching related hw_event table.
 * A value of -1 means 'not supported',
 * any other value means the raw hw_event ID.
 */
static const int nps_cache_event_map[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	[C(L1D)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= 54, /* dclm */
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= 55, /* dcsm */
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(L1I)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= 48, /* icm */
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(LL)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(DTLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= 69, /* edtlb */
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(ITLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= 68, /* eitlb */
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(BPU)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
			[C(RESULT_MISS)]	= PERF_COUNT_HW_BRANCH_MISSES,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(NODE)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
};

static int nps_map_hw_event(u64 config);
static int nps_map_cache_event(u64 config);

static const struct nps_pmu npspmu = {
	.counter_size		= NPS_PMU_COUNTER_SIZE,
	.n_counters		= NPS_MAX_COUNTERS,
	.hw_events		= nps_hw_event_map,
	.cache_events		= &nps_cache_event_map,
	.map_hw_event		= nps_map_hw_event,
	.map_cache_event	= nps_map_cache_event,
	.max_events		= ARRAY_SIZE(nps_hw_event_map),
	.max_period		= (1ULL << 31) - 1,
	.overflow		= 1ULL << 31,
};

static const struct nps_pmu *nps_pmu __read_mostly;

/*
 * Map generic hardware events to NPS PMU.
 */
static int nps_map_hw_event(u64 config)
{
	if (config >= nps_pmu->max_events)
		return -EINVAL;
	return nps_pmu->hw_events[config];
}

/*
 * Map generic hardware cache events to NPS PMU.
 */
static int nps_map_cache_event(u64 config)
{
	unsigned int cache_type, cache_op, cache_result;
	int code;

	cache_type	= (config >>  0) & 0xff;
	cache_op	= (config >>  8) & 0xff;
	cache_result	= (config >> 16) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return -EINVAL;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return -EINVAL;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	code = (*nps_pmu->cache_events)[cache_type][cache_op][cache_result];

	if (code == CACHE_OP_UNSUPPORTED)
		return -ENOENT;

	return code;
}

/*
 * Write Performance Counters.
 */
static void nps_write_counter(unsigned int idx, u32 val)
{
	WARN_ONCE(val & nps_pmu->overflow,
			"Writing overflowed value (0x%x) to perf counter%d\n",
			val, idx);

	switch (idx) {
	case 0:
		write_aux_reg(NPS_REG_PCT_COUNT0, val);
		break;
	case 1:
		write_aux_reg(NPS_REG_PCT_COUNT1, val);
		break;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		break;
	}
}

/*
 * Write Performance Counters Config.
 */
static void nps_write_config(unsigned int idx, u32 val)
{
	switch (idx) {
	case 0:
		write_aux_reg(NPS_REG_PCT_CONFIG0, val);
		break;
	case 1:
		write_aux_reg(NPS_REG_PCT_CONFIG1, val);
		break;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		break;
	}
}

/*
 * Read Performance Counters.
 */
static uint64_t nps_read_counter(int idx)
{
	uint32_t result = 0;

	switch (idx) {
	case 0:
		result = read_aux_reg(NPS_REG_PCT_COUNT0);
		break;
	case 1:
		result = read_aux_reg(NPS_REG_PCT_COUNT1);
		break;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		break;
	}
	return result;
}

/*
 * Propagate event elapsed time into the generic event.
 * Can only be executed on the CPU where the event is active.
 */
static void nps_perf_event_update(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	uint64_t prev_raw_count, new_raw_count;
	int64_t delta;

	do {
		prev_raw_count = local64_read(&hwc->prev_count);
		new_raw_count = nps_read_counter(idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
				 new_raw_count) != prev_raw_count);

	delta = (new_raw_count - prev_raw_count) &
		((1ULL << nps_pmu->counter_size) - 1ULL);

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

}

static void nps_pmu_read(struct perf_event *event)
{
	nps_perf_event_update(event);
}

static int _nps_pmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int code;

	switch (event->attr.type) {
	case PERF_TYPE_HARDWARE:
		code = nps_pmu->map_hw_event(event->attr.config);
		if (event->attr.config >= PERF_COUNT_HW_MAX)
			return -ENOENT;
		break;
	case PERF_TYPE_HW_CACHE:
		code = nps_pmu->map_cache_event(event->attr.config);
		break;
	case PERF_TYPE_RAW:
		code = event->attr.config;
		break;
	default:
		/* Should not happen. */
		return -EOPNOTSUPP;
	}

	/* Return early if the event is unsupported */
	if (code < 0)
		return code;

	/*
	 * Don't assign an index until the event is placed into the hardware.
	 * -1 signifies that we're still deciding where to put it. On SMP
	 * systems each core has its own set of counters, so we can't do any
	 * constraint checking yet.
	 */
	hwc->config = code;
	hwc->idx = -1;

	pr_debug("initializing event %d with cfg %d\n",
		 (int) event->attr.config, (int) hwc->config);

	/*
	 * For non-sampling runs
	 */
	if (!hwc->sample_period) {
		hwc->sample_period = nps_pmu->max_period;
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	return 0;
}

/* Initializes hw_perf_event structure if event is supported */
static int nps_pmu_event_init(struct perf_event *event)
{
	/* does not support taken branch sampling? */
	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	switch (event->attr.type) {
	case PERF_TYPE_RAW:
	case PERF_TYPE_HARDWARE:
	case PERF_TYPE_HW_CACHE:
		break;
	default:
		return -ENOENT;
	}

	return _nps_pmu_event_init(event);
}

/* Starts all counters */
static void nps_pmu_enable(struct pmu *pmu)
{
	struct nps_reg_pct_control ctrl;

	ctrl.value = read_aux_reg(NPS_REG_PCT_CONTROL);
	ctrl.n = 0;
	ctrl.condition = 1;
	write_aux_reg(NPS_REG_PCT_CONTROL, ctrl.value);
}

/* Stops all counters */
static void nps_pmu_disable(struct pmu *pmu)
{
	struct nps_reg_pct_control ctrl;

	ctrl.value = read_aux_reg(NPS_REG_PCT_CONTROL);
	ctrl.n = 0;
	ctrl.condition = 0;
	write_aux_reg(NPS_REG_PCT_CONTROL, ctrl.value);
}

static int nps_event_set_period(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int ret = 0;

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	WARN_ONCE(left <= 0, "Invalid left period (<=0)\n");

	if (left > nps_pmu->max_period) {
		left = nps_pmu->max_period;
		local64_set(&hwc->period_left, left);
	}

	local64_set(&hwc->prev_count, nps_pmu->overflow - left);

	nps_write_counter(idx, nps_pmu->overflow - left);

	perf_event_update_userpage(event);

	return ret;
}

/*
 * Start an event (without re-assigning counter)
 */
static void nps_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct nps_reg_pct_config config;
	int idx = hwc->idx;

	if (WARN_ON_ONCE(!(event->hw.state & PERF_HES_STOPPED)))
		return;

	if (WARN_ON_ONCE(idx < 0 || idx >= nps_pmu->n_counters))
		return;

	if (flags & PERF_EF_RELOAD) {
		WARN_ON_ONCE(!(event->hw.state & PERF_HES_UPTODATE));
		/* Set the period for the event. */
		nps_event_set_period(event);
	}

	event->hw.state = 0;

	config.value = 0;
	config.um = !event->attr.exclude_user;
	config.km = !event->attr.exclude_kernel;
	config.condition = hwc->config;
	config.ie = 1;
	config.ofsel = 1;
	config.tid = read_aux_reg(CTOP_AUX_THREAD_ID);

	/* enable NPS pmu here */
	nps_write_config(idx, config.value);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);

}

static void nps_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (!(event->hw.state & PERF_HES_STOPPED)) {
		/* stop NPS pmu here */
		/* condition code #0 is always "never" */
		WARN_ON_ONCE(hwc->state & PERF_HES_STOPPED);

		nps_write_config(idx, 0);

		event->hw.state |= PERF_HES_STOPPED;
	}

	if ((flags & PERF_EF_UPDATE) &&
	    !(event->hw.state & PERF_HES_UPTODATE)) {
		nps_perf_event_update(event);
		event->hw.state |= PERF_HES_UPTODATE;
	}
}

static void nps_pmu_del(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	nps_pmu_stop(event, PERF_EF_UPDATE);
	cpuc->events[event->hw.idx] = NULL;
	__clear_bit(event->hw.idx, cpuc->used_mask);

	perf_event_update_userpage(event);
}

/*
 * Add a single event to the PMU.
 */
static int nps_pmu_add(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	int err = 0;

	perf_pmu_disable(event->pmu);

	if (idx < 0 || idx >= nps_pmu->n_counters ||
			__test_and_set_bit(idx, cpuc->used_mask)) {
		idx = find_first_zero_bit(cpuc->used_mask,
					  nps_pmu->n_counters);
		if (idx == nps_pmu->n_counters) {
			err = -EAGAIN;
			goto out;
		}

		__set_bit(idx, cpuc->used_mask);
		hwc->idx = idx;
	}

	cpuc->events[idx] = event;

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (!(flags & PERF_EF_START))
		hwc->state |= PERF_HES_ARCH;

	/* Make sure the counter is disabled */
	nps_write_counter(idx, 0);
	nps_write_config(idx, 0);

	/*
	* Start if requested.
	*/
	if (flags & PERF_EF_START)
		nps_pmu_start(event, PERF_EF_RELOAD);

out:
	perf_pmu_enable(event->pmu);
	return err;
}

static void handle_associated_event(struct cpu_hw_events *cpuc,
				    int idx, struct perf_sample_data *data,
				    struct pt_regs *regs)
{
	struct perf_event *event = cpuc->events[idx];

	nps_perf_event_update(event);

	data->period = event->hw.last_period;

	if (!nps_event_set_period(event))
		return;

	if (perf_event_overflow(event, data, regs))
		nps_pmu_stop(event, 0);
}

static void nps_pmu_overflow_handler(struct pt_regs *regs)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct perf_sample_data data;
	u64 counter;
	int n;

	perf_sample_data_init(&data, 0, 0);

	for (n = 0; n < nps_pmu->n_counters; n++) {
		if (test_bit(n, cpuc->used_mask)) {
			counter = nps_read_counter(n);
			if (counter & nps_pmu->overflow)
				handle_associated_event(cpuc, n, &data, regs);
		}
	}
}

static struct pmu eznps_pmu = {
	.pmu_enable	= nps_pmu_enable,
	.pmu_disable	= nps_pmu_disable,
	.event_init	= nps_pmu_event_init,
	.add		= nps_pmu_add,
	.del		= nps_pmu_del,
	.start		= nps_pmu_start,
	.stop		= nps_pmu_stop,
	.read		= nps_pmu_read,
};

static int nps_pmu_device_probe(struct platform_device *pdev)
{
	nps_pmu = &npspmu;

	pr_info("NPS PMU set with %d counters of size %d bits\n",
		nps_pmu->n_counters, nps_pmu->counter_size);

	return perf_pmu_register(&eznps_pmu, pdev->name, PERF_TYPE_RAW);
}

/*
 * Entry point for EV_PerfMon exception
 */
void do_perfmon(struct pt_regs *regs)
{
	unsigned long pflags;

	nmi_enter();
	perf_control_save(pflags);

	nps_pmu_overflow_handler(regs);

	perf_control_restore(pflags);
	nmi_exit();
}

#ifdef CONFIG_OF
static const struct of_device_id nps_pmu_match[] = {
	{ .compatible = "ezchip,nps-pct" },
	{},
};
MODULE_DEVICE_TABLE(of, nps_pmu_match);
#endif

static struct platform_driver nps_pmu_driver = {
	.driver	= {
		.name		= "nps-pct",
		.of_match_table = of_match_ptr(nps_pmu_match),
	},
	.probe		= nps_pmu_device_probe,
};

module_platform_driver(nps_pmu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EZchip Semiconductor");
