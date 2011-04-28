#
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2003 by ARC International
# Addition of S3C4530 by Arcturus Networks Inc.
#
.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -c $< -o $*.o

#all: head.o entry.o proc.o
#O_TARGET := proc.o

obj-y += entry.o process.o signal.o traps.o

ifdef CONFIG_SMP
obj-y += smp.o idu.o
endif

extra-y += head.o

head.o: head.S

entry.o: entry.S

#include $(TOPDIR)/Rules.make
