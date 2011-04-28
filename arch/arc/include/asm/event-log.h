/*************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 *  vineetg: Feb 2008
 *
 *      System Event Logging APIs
 *
 ************************************************************************/

#ifndef __ASM_ARC_EVENT_LOG_H
#define __ASM_ARC_EVENT_LOG_H

/*######################################################################
 *
 *    Event Logging API
 *
 *#####################################################################*/

#define SYS_EVENT(x)        (1 << x)

#define CUSTOM_EVENT(x)     (0x01000000 * x)

#define MAX_SNAPS  1024
#define MAX_SNAPS_MASK (MAX_SNAPS-1)

#define SNAP_INTR_IN            SYS_EVENT(0)
#define SNAP_EXCP_IN            SYS_EVENT(1)
#define SNAP_TRAP_IN            SYS_EVENT(2)
#define SNAP_INTR_IN2           SYS_EVENT(3)

#define SNAP_OUT                0xF00
#define SNAP_INTR_OUT           SYS_EVENT(8)
#define SNAP_EXCP_OUT           SYS_EVENT(9)
#define SNAP_TRAP_OUT           SYS_EVENT(10)
#define SNAP_INTR_OUT2          SYS_EVENT(11)

#define SNAP_EXCP_OUT_FAST      SYS_EVENT(12)

#define SNAP_PRE_CTXSW_2_U      CUSTOM_EVENT(1)
#define SNAP_PRE_CTXSW_2_K      CUSTOM_EVENT(2)
#define SNAP_DO_PF_ENTER        CUSTOM_EVENT(3)
#define SNAP_DO_PF_EXIT         CUSTOM_EVENT(4)
#define SNAP_TLB_FLUSH_ALL      CUSTOM_EVENT(5)
#define SNAP_PREEMPT_SCH_IRQ    CUSTOM_EVENT(6)
#define SNAP_PREEMPT_SCH        CUSTOM_EVENT(7)

#define SNAP_SENTINEL           CUSTOM_EVENT(8)


#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

#define take_snap(type,extra,ptreg)
#define sort_snaps(halt_after_sort)

#else

#ifndef __ASSEMBLY__

typedef struct {

    /*  0 */ char nm[16];
    /* 16 */ unsigned int extra; /* Traps: Sys call num,
                                    Intr: IRQ, Excepn:
                                  */
    /* 20 */ unsigned int  fault_addr;
    /* 24 */ unsigned int  cause;
    /* 28 */ unsigned int task;
    /* 32 */ unsigned long time;
    /* 36 */ unsigned int  event;
    /* 40 */ unsigned int  sp;
    /* 44 */ unsigned int  extra2;
    /* 40 */ unsigned int  extra3;

}
timeline_log_t;

void take_snap(int type, unsigned int extra, unsigned int extra2);
void sort_snaps(int halt_after_sort);

#endif	/* __ASSEMBLY__ */

#endif /* CONFIG_ARC_DBG_EVENT_TIMELINE */

/*######################################################################
 *
 *    Process fork/exec Logging
 *
 *#####################################################################*/

#ifndef __ASSEMBLY__

//#define CONFIG_DEBUG_ARC_PID 1

#ifndef CONFIG_DEBUG_ARC_PID

#define fork_exec_log(p, c)

#endif /* CONFIG_DEBUG_ARC_PID */

#endif /* __ASSEMBLY__ */

#endif /* __ASM_ARC_EVENT_PROFILE_H */
