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
 * linux/arch/arc/mm/extable.c
 * Taken from MIPS
 */
#include <linux/module.h>
#include <asm/uaccess.h>

int fixup_exception(struct pt_regs *regs)
{
	const struct exception_table_entry *fixup;

	fixup = search_exception_tables(instruction_pointer(regs));
	if (fixup) {
		regs->ret = fixup->nextinsn;

		return 1;
	}

	return 0;
}
