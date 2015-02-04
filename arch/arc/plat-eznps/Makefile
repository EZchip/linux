#
# Makefile for the linux kernel.
#

KBUILD_CFLAGS += -Iarch/arc/plat-eznps/include

obj-y := platform.o irq.o dpl.o ctop.o atomic64.o
obj-$(CONFIG_SMP) += time.o smp.o
obj-$(CONFIG_EZNPS_MTM_EXT) += mtm.o
obj-$(CONFIG_PERF_EVENTS) += perf_event.o
