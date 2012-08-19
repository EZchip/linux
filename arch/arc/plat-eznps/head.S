/*******************************************************************************

  EZNPS CPU startup Code
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#include <asm/asm-offsets.h>
#include <asm/entry.h>
#include <linux/linkage.h>
#include <asm/arcregs.h>

.cpu A7

;----------------------------------------------------------------
; First lines of code run by secondary before jumping to 'C'
;----------------------------------------------------------------
.section .init.text, "ax",@progbits
.type ez_first_lines_of_secondary, @function
.globl ez_first_lines_of_secondary
.align 1024

ez_first_lines_of_secondary:

j first_lines_of_secondary

