/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Amit Bhor: Codito Technologies 2004
 */

#ifndef _ASM_ARC_STRING_H
#define _ASM_ARC_STRING_H

#define __HAVE_ARCH_MEMSET

extern void * memset(void *, int, __kernel_size_t);

#define memzero(buff, len)	memset(buff, 0, len)

#define __HAVE_ARCH_MEMCPY

extern void * memcpy(void *, const void *, __kernel_size_t);



#endif
