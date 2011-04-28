#
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2003 by ARC International
#
# arch/arc/Makefile
#
# Author : Sameer Dhavale
#


# Its a rewrite from old arcch/arc/proc/arc700/Rules.make

ifdef CONFIG_ARCH_ARC700
PROCESSOR  := arc700
endif

KBUILD_DEFCONFIG := arc_defconfig

PROCESSOR  := arc700

#LDFLAGS_vmlinux :=-X -T arch/$(ARCH)/kernel/vmlinux.lds
GZFLAGS		:=-9
KBUILD_CFLAGS		+= -mA7 -mlong-calls -fno-common -pipe -fno-builtin -D__linux__
KBUILD_AFLAGS		+= $(KBUILD_CFLAGS)

MODFLAGS	+= -mlong-calls

KBUILD_CFLAGS	+= $(shell if [ $(call cc-version, $(CC)) -gt 0400 ] ; then echo $(call cc-option,-mno-sdata); fi ;)

gcc-ver := $(call cc-version, $(CC))

ifeq ($(CONFIG_ARC_MIXED_CODE),y)
KBUILD_CFLAGS		+= -mmixed-code
endif

ifeq ($(gcc-ver), 0304)
KBUILD_CFLAGS		+= -fno-unit-at-a-time
endif

ifeq ($(CONFIG_DEBUG_INFO),y)
KBUILD_CFLAGS		+= -O1 -gdwarf-2
endif

ifdef CONFIG_ARC_STACK_UNWIND
KBUILD_CFLAGS		+= -fasynchronous-unwind-tables
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
KBUILD_CFLAGS    	+= -mbig-endian
KBUILD_AFLAGS		+= -mbig-endian
LINKFLAGS	+= -EB
LDFLAGS		+= -EB
endif

LIBGCC		:= $(shell $(CC) $(KBUILD_CFLAGS) --print-libgcc-file-name)

TEXTADDR	 = 0x80002000

# Sameer: We don't need this now. Its replaced by vmlinux-lds in
#         top level Makefile in 2.6
#LDSCRIPT         = arch/$(ARCH)/proc/$(PROCESSOR)/linux.lds.in

export	LDFLAGS LIBGCC TEXTADDR GZFLAGS

head-y		:= arch/arc/proc/$(PROCESSOR)/head.o \
		   arch/arc/kernel/init_task.o

core-y		+= arch/arc/kernel/ arch/arc/mm/ \
			arch/arc/proc/$(PROCESSOR)/

ifdef CONFIG_OPROFILE
core-y		+= arch/arc/oprofile/
endif

drivers-y	+= arch/arc/drivers/

libs-y 		+= arch/arc/lib/ $(LIBGCC)

ifeq ($(CONFIG_ARC700_HLINK),y)
libs-y		+= arch/arc/lib/libhlt.a
endif

#default target for make without any arguements.
KBUILD_IMAGE := bootpImage

all:	$(KBUILD_IMAGE)

boot := arch/arc/boot

# Sameer: People update symlinks to machine and proc folders
#         here. We wil need it when we'll add more processors
#         or boards.

archprepare: maketools

PHONY += maketools FORCE

maketools: include/linux/version.h FORCE
	$(Q)$(MAKE) $(build)=arch/$(ARCH)/tools

# Convert bzImage to zImage
bzImage: zImage



# work around for bug 94179: force some object file to have load,alloc flags
# set for the debug_frame section

ifdef CONFIG_ARC_STACK_UNWIND
objconvert: init/do_mounts.o
	$(Q) $(OBJCOPY) --set-section-flags .debug_frame=load,alloc init/do_mounts.o init/do_mounts.o
else
objconvert:
endif   #CONFIG_ARC_STACK_UNWIND

zImage Image xipImage bootpImage uImage: objconvert vmlinux
#	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@


bootpImage: vmlinux
#	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@


CLEAN_FILES	+= \
	arch/$(ARCH)/proc/$(PROCESSOR)/linux.lds \

MRPROPER_FILES	+= \
	include/asm-$(ARCH)/arch \
	include/asm-$(ARCH)/proc \

# We use MRPROPER_FILES and CLEAN_FILES now
archclean:
	$(Q)$(MAKE) $(clean)=$(boot)
