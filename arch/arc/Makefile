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

UTS_MACHINE := arc
PROCESSOR  := arc700

KBUILD_DEFCONFIG := aa4_defconfig

PLATFORM := AA4
export PLATFORM

#LDFLAGS_vmlinux :=-X -T arch/$(ARCH)/kernel/vmlinux.lds
GZFLAGS		:=-9
KBUILD_CFLAGS		+= -Iarch/arc/include/asm/board-$(PLATFORM)
KBUILD_CFLAGS		+= -mA7 -fno-common -pipe -fno-builtin \
						-D__linux__  -fcall-used-gp -multcost=16

# Idea is to use -O3 for our builds (Default kernel makefile uses -O2)
# However doing that unconditionally here breaks -Os:
#    our -O3 over-rides -Os set by top-level-makefile
#
ifndef CONFIG_CC_OPTIMIZE_FOR_SIZE
KBUILD_CFLAGS  += -O3
endif

# vineetg, Oct 2009
# Low level event tracing is sort of crude, as it relies on visualisation
# of log-buffer as a global array in Metaware debugger's "globals" Pane and
# then manually saving it to a file. However w/o -g, debugger can't show the
# non-scalar globals, hence this switch.
# Ideally we must use something like relay FS, but can't, because this tracing
# is generally done in case of weird faults like Machine check, to capture the
# dying kernel's last few events. Thus expecting multiple kernel/user
# daemons to haul data out of kernel in that state would be really stupid !
ifeq ($(CONFIG_ARC_DBG_EVENT_TIMELINE),y)
KBUILD_CFLAGS += -g
endif

MODFLAGS	+= -mlong-calls

KBUILD_CFLAGS	+= $(shell if [ $(call cc-version, $(CC)) -gt 0400 ] ; then echo $(call cc-option,-mno-sdata); fi ;)

gcc-ver := $(call cc-version, $(CC))
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

ifdef CONFIG_CC_OPTIMIZE_FOR_SIZE
KBUILD_CFLAGS		+= -DNONINLINE_USR_CPY
endif

#setup AFLAGS from CFLAGS when we are done with all the CFLAGS
KBUILD_AFLAGS		+= $(KBUILD_CFLAGS)

LIBGCC		:= $(shell $(CC) $(KBUILD_CFLAGS) --print-libgcc-file-name)

TEXTADDR	 = 0x80002000

export	LDFLAGS LIBGCC TEXTADDR GZFLAGS

head-y		:= arch/arc/proc/$(PROCESSOR)/head.o \
		   arch/arc/kernel/init_task.o

core-y		+= arch/arc/kernel/ arch/arc/mm/ \
			arch/arc/proc/$(PROCESSOR)/

# vineetg Feb 2010:
# We switched to -mno-long-calls for overall kernel build
# However lib/decompress_inflate.o (.init.text) calls
# zlib_inflate_workspacethere (.text) causing relocation error.
# Thus forcing all exten calls in this file to be long calls
export CFLAGS_decompress_inflate.o = -mlong-calls

core-$(CONFIG_OPROFILE)		+= arch/arc/oprofile/

drivers-y	+= arch/arc/drivers/

libs-y 		+= arch/arc/lib/ $(LIBGCC)

#default target for make without any arguements.
KBUILD_IMAGE := bootpImage

all:	$(KBUILD_IMAGE)

boot := arch/arc/boot

# Sameer: People update symlinks to machine and proc folders
#         here. We wil need it when we'll add more processors
#         or boards.

archprepare: maketools arch/arc/include/asm/board

PHONY += maketools FORCE

maketools: include/linux/version.h FORCE
	$(Q)$(MAKE) $(build)=arch/$(ARCH)/tools

define arc-create-symlink
	@echo 'SYMLINK arch/arc/include/asm/board -> arch/arc/include/asm/board-$(PLATFORM)'; \
	ln -fsn board-$(PLATFORM) arch/arc/include/asm/board;
endef

arch/arc/include/asm/board: FORCE
	$(arc-create-symlink)

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
