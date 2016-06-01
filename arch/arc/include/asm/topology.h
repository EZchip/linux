#ifndef _ASM_ARC_TOPOLOGY_H
#define _ASM_ARC_TOPOLOGY_H

#ifdef CONFIG_NUMA
#define cpu_to_node(cpu)	((void)(cpu), 0)
#define parent_node(node)	(node)
#define cpumask_of_node(node)	((void)node, cpu_online_mask)
#endif

#ifdef CONFIG_NPS_CPU_TOPOLOGY

#include <linux/cpumask.h>

struct cputopo_nps {
	int thread_id;
	int core_id;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
};

extern struct cputopo_nps cpu_topology[NR_CPUS];

#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_sibling_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);

#else

static inline void init_cpu_topology(void) { }
static inline void store_cpu_topology(unsigned int cpuid) { }

#endif

#include <asm-generic/topology.h>

#endif /* _ASM_ARC_TOPOLOGY_H */
