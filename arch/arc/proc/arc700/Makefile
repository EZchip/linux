# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2009 by ARC International
# Addition of S3C4530 by Arcturus Networks Inc.
#

obj-y += entry.o process.o signal.o traps.o

#----------------------------------------------------------------------
# for unwinding from __switch_to, we need to write it in "C"
# otherwise hand written assembler is a more "natural" way to code it
#
ifdef CONFIG_ARC_STACK_UNWIND
CFLAGS_ctx_sw.o += -fno-omit-frame-pointer
obj-y += ctx_sw.o
else
obj-y += ctx_sw_asm.o
endif
#---------------------------------------------------------------------

ifdef CONFIG_SMP
obj-y += smp.o idu.o
endif

extra-y += head.o

head.o: head.S

entry.o: entry.S
