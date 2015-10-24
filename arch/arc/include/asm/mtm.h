/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_MTM_H
#define _ASM_ARC_MTM_H

#ifdef __ASSEMBLY__

#ifdef CONFIG_EZNPS_MTM_EXT
#include <plat/ctop.h>

.macro  HW_SCHD_OFF_R4   dst, offset
	st  r4, [\dst, \offset]
	.word CTOP_INST_HWSCHD_OFF_R4
.endm

.macro  HW_SCHD_RESTORE_R4   dst, offset
	.word CTOP_INST_HWSCHD_RESTORE_R4
	ld  r4, [\dst, \offset]
.endm
#else

.macro  HW_SCHD_OFF_R4   dst, offset
.endm

.macro  HW_SCHD_RESTORE_R4   dst, offset
.endm

#endif /* CONFIG_EZNPS_MTM_EXT */

#else

#ifdef CONFIG_EZNPS_MTM_EXT
#include <soc/nps/mtm.h>
#else
#define DEFINE_SCHD_FLAG(type, name)
#define hw_schd_save(flags)
#define hw_schd_restore(flags)
#endif /* CONFIG_EZNPS_MTM_EXT */

#endif /* __ASSEMBLY__ */
#endif /* _ASM_ARC_MTM_H */
