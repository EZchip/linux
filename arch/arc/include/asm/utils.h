/*************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 ************************************************************************/

#ifndef __ASM_ARC_UTILS_H
#define __ASM_ARC_UTILS_H

#include <linux/kallsyms.h>

extern char *arc_identify_sym(unsigned long addr);
extern char *arc_identify_sym_r(unsigned long addr, char *namebuf, size_t sz);

#endif
