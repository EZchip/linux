/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 *  linux/include/asm-arcnommu/shmparam.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor
 */

#ifndef _ASM_ARC_SHMPARAM_H
#define _ASM_ARC_SHMPARAM_H
#include <asm/cache.h>
/*
 * This should be the size of the virtually indexed cache/ways,
 * or page size, whichever is greater since the cache aliases
 * every size/ways bytes.
 */
#define SHMLBA ARC_shmlba

#endif /* _ASM_ARC_SHMPARAM_H */
