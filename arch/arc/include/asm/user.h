#ifndef _ASM_ARC_USER_H
#define _ASM_ARC_USER_H

/* When the kernel dumps core, it starts by dumping the user struct -
   this will be used by gdb to figure out where the data and stack segments
   are within the file, and what virtual addresses to use. */

#ifdef CONFIG_HAVE_HW_BREAKPOINT

#define ARC_MAX_HBP_SLOTS 8

struct user {
	unsigned long int	u_dr_value[ARC_MAX_HBP_SLOTS];
	unsigned long int	u_dr_control[ARC_MAX_HBP_SLOTS];
	unsigned long int	u_dr_mask[ARC_MAX_HBP_SLOTS];
};
#endif	/* CONFIG_HAVE_HW_BREAKPOINT */
#endif /* _ASM_ARC_USER_H */
