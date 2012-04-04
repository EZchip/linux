/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_SERIAL_H
#define _ASM_ARC_SERIAL_H

#if defined(CONFIG_ARC_SERIAL) && defined(CONFIG_EARLY_PRINTK)
extern void __init arc_early_serial_reg(void);
#else
#define arc_early_serial_reg(void);
#endif

extern unsigned long serial_baudrate;

#include <asm-generic/serial.h>

#endif  /* _ASM_ARC_SERIAL_H */
