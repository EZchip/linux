#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Copyright (C) 2003, ARC International
#

obj-$(CONFIG_MODULES)   += arcksyms.o module.o
# Object file lists.

#
# Pass UTS_MACHINE for user_regset definition
#
CFLAGS_ptrace.o		+= -DUTS_MACHINE='"$(UTS_MACHINE)"'

obj-y		+= arcksyms.o ptrace.o sys_arc.o setup.o irq.o
obj-y		+= troubleshoot.o stacktrace.o event-log.o
obj-$(CONFIG_ARCH_ARC_HLINK)		+= arc_hostlink.o
obj-$(CONFIG_XILINX_TEMAC) += platform.o
obj-$(CONFIG_ISS_MAC) += platform.o
ifdef CONFIG_SMP
obj-y		+= atomic_lock.o bitops_lock.o
endif

ifeq ($(CONFIG_GENERIC_CLOCKEVENTS),y)
    obj-y += time.o
else
    obj-y += time-jiff.o
endif

ifeq ($(CONFIG_EARLY_PRINTK),y)
    obj-y += early_printk.o
endif

obj-m		:=
obj-n		:=
obj-		:=

ifdef CONFIG_ARC_STACK_UNWIND
obj-y		+= unwind.o
endif

ifdef CONFIG_KPROBES
obj-y       += kprobes.o
endif

extra-y := init_task.o vmlinux.lds
