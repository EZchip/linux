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
 *  linux/include/asm-arc/serial.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_SERIAL_H
#define _ASM_ARC_SERIAL_H

#define BASE_BAUD CONFIG_ARC700_SERIAL_BAUD

void __init reset_vuart(void);

#endif  /* _ASM_ARC_SERIAL_H */
