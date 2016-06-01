/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_MMZONE_H
#define _ASM_ARC_MMZONE_H

#ifdef CONFIG_DISCONTIGMEM
extern struct pglist_data node_data[];
#define NODE_DATA(nid) (&node_data[nid])

static inline int pfn_to_nid(unsigned long pfn)
{
	if (pfn >= ARCH_PFN_OFFSET)
		return 0;

	return 1;
}

static inline int pfn_valid(unsigned long pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}
#endif /* CONFIG_DISCONTIGMEM  */

#endif
