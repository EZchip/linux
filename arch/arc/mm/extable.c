/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Borrowed heavily from MIPS
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

#ifdef NONINLINE_USR_CPY
/*
  documentation says that copy_from_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of copy_from_user consider it an error
  when a positive value is returned
*/
unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{

    if(access_ok(VERIFY_READ, from, n)) {
        return (__generic_copy_from_user(to, from, n));
    }
    return n;
}

/*
  documentation says that __copy_from_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of __copy_from_user consider it an error
  when a positive value is returned
*/
unsigned long
__copy_from_user(void *to, const void *from, unsigned long n)
{
    return (__generic_copy_from_user(to, from, n));
}

/*
  documentation says that copy_to_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of copy_to_user consider it an error
  when a positive value is returned
*/
unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
    if(access_ok(VERIFY_READ, to, n))
        return (__generic_copy_to_user(to, from, n));
    return n;
}

/*
  documentation says that __copy_to_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of __copy_to_user consider it an error
  when a positive value is returned
*/
unsigned long
__copy_to_user(void *to, const void *from, unsigned long n)
{
    return(__generic_copy_to_user(to, from, n));
}

#endif
