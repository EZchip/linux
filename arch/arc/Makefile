#
# Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

UTS_MACHINE := arc
PROCESSOR  := arc700

KBUILD_DEFCONFIG := aa4_defconfig

PLATFORM := AA4
export PLATFORM

KBUILD_CFLAGS		+= -Iarch/arc/include/asm/board-$(PLATFORM)
KBUILD_CFLAGS		+= -mA7 -fno-common -pipe -fno-builtin \
						-D__linux__

# preprocessor macros due to following: __Xlock __Xswape __Xrtsc
ifdef CONFIG_ARC_HAS_LLSC
KBUILD_CFLAGS		+= -mlock
endif
ifdef CONFIG_ARC_HAS_SWAPE
KBUILD_CFLAGS		+= -mswape
endif
ifdef CONFIG_ARC_HAS_RTSC
KBUILD_CFLAGS		+= -mrtsc
endif

ifdef CONFIG_CC_OPTIMIZE_FOR_SIZE
# Generic build system already passes -Os
# Enable some explicitly uninlined routines
KBUILD_CFLAGS		+= -DNONINLINE_USR_CPY -DNONINLINE_MEMSET
# Generic build system uses -O2, we want -O3
else
KBUILD_CFLAGS  += -O3
endif

# small data is default for elf32 tool-chain. If not usable, disable it
# This also allows repurposing GP as scratch reg to gcc reg allocator
enable_small_data := n
ifeq ($(enable_small_data),n)
KBUILD_CFLAGS  += -mno-sdata -fcall-used-gp
endif

# vineetg Oct 2010: ARC GCC 4.4.2 supports section anchors
KBUILD_CFLAGS		+=	$(call cc-ifversion, -gt, 0402, -fsection-anchors)

# vineetg, Oct 2009
# Low level event tracing is sort of crude, as it relies on visualisation
# of log-buffer as a global array in Metaware debugger's "globals" Pane and
# then manually saving it to a file. However w/o -g, debugger can't show the
# non-scalar globals, hence this switch.
# Ideally we must use something like relay FS, but can't, because this tracing
# is generally done in case of weird faults like Machine check, to capture the
# dying kernel's last few events. Thus expecting multiple kernel/user
# daemons to haul data out of kernel in that state would be really stupid !
ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
KBUILD_CFLAGS += -g
endif

MODFLAGS	+= -mlong-calls

ifdef CONFIG_ARC_STACK_UNWIND
KBUILD_CFLAGS		+= -fasynchronous-unwind-tables
endif

LINUXINCLUDE +=  -include ${src}/arch/arc/include/asm/defines.h \
				 -include ${src}/arch/arc/include/asm/current.h

ifdef CONFIG_CPU_BIG_ENDIAN
KBUILD_CFLAGS    	+= -mbig-endian
LDFLAGS		+= -EB
endif

ifndef CONFIG_ARC_USE_HW_MPY
KBUILD_CFLAGS	+= -mno-mpy
# newlib for ARC700 assumes MPY to be always present, which is generally true
# However, if someone really doesn't want MPY, we need to use the 600 ver
# which coupled with -mno-mpy will use mpy emulation
ARC_LIBGCC		:= -marc600
# Also need to hint gcc that mpy would be extremely costly
KBUILD_CFLAGS   += -multcost=30
else
KBUILD_CFLAGS	+= -multcost=16
ARC_LIBGCC		:= -mA7
endif

#setup AFLAGS from CFLAGS when we are done with all the CFLAGS
KBUILD_AFLAGS		+= $(KBUILD_CFLAGS)

LIBGCC		:= $(shell $(CC) $(ARC_LIBGCC) --print-libgcc-file-name)

TEXTADDR	 = 0x80002000

head-y		:= arch/arc/proc/$(PROCESSOR)/head.o arch/arc/kernel/init_task.o
core-y		+= arch/arc/kernel/ arch/arc/mm/ arch/arc/proc/$(PROCESSOR)/
core-$(CONFIG_OPROFILE)		+= arch/arc/oprofile/
drivers-y	+= arch/arc/drivers/
libs-y 		+= arch/arc/lib/ $(LIBGCC)

#default target for make without any arguements.
KBUILD_IMAGE := bootpImage

all:	$(KBUILD_IMAGE)

bootpImage uImage: objconvert vmlinux

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

archprepare: arch/arc/include/asm/board

arch/arc/include/asm/board: FORCE
	$(arc-create-symlink)

define arc-create-symlink
	@echo 'SYMLINK arch/arc/include/asm/board -> ${src}/arch/arc/include/asm/board-$(PLATFORM)'; \
	ln -fsn board-$(PLATFORM) ${src}/arch/arc/include/asm/board;
endef

# work around for bug 94179: force some object file to have load,alloc flags
# set for the debug_frame section

ifdef CONFIG_ARC_STACK_UNWIND
objconvert: init/do_mounts.o
	$(Q) $(OBJCOPY) --set-section-flags .debug_frame=load,alloc init/do_mounts.o init/do_mounts.o
else
objconvert:
endif

# vineetg Feb 2010:
# We switched to -mno-long-calls for overall kernel build
# However lib/decompress_inflate.o (.init.text) calls
# zlib_inflate_workspacethere (.text) causing relocation error.
# Thus forcing all exten calls in this file to be long calls
export CFLAGS_decompress_inflate.o = -mlong-calls
