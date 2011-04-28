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
 *  linux/include/asm-arc/statfs.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_STATFS_H
#define _ASM_ARC_STATFS_H

#ifndef __KERNEL_STRICT_NAMES

#include <linux/types.h>

typedef __kernel_fsid_t	fsid_t;

#endif

/* Sameer: Commenting old def to make way for a new one. Couple of new
           structure members are required in linux-2.6. */
#define f_fstyp f_type
struct statfs {
	long		f_type;
	long		f_bsize;
	long		f_blocks;
	long		f_bfree;
	long		f_bavail;
	long		f_files;
	long		f_ffree;

	/* Linux specials */
	__kernel_fsid_t	f_fsid;
	long		f_namelen;
	long		f_spare[6];

	long		f_frsize;	/* Fragment size - unsupported */

};

/* struct statfs { */
/* 	long f_type; */
/* 	long f_bsize; */
/* 	long f_blocks; */
/* 	long f_bfree; */
/* 	long f_bavail; */
/* 	long f_files; */
/* 	long f_ffree; */
/* 	__kernel_fsid_t f_fsid; */
/* 	long f_namelen; */
/* 	long f_spare[6]; */
/* }; */

/* Sameer: We need to relook at this definition
           later. */
/*
 * Unlike the traditional version the LFAPI version has none of the ABI junk
 */
struct statfs64 {
	__u32	f_type;
	__u32	f_bsize;
	__u32	f_frsize;	/* Fragment size - unsupported */
	__u32	__pad;
	__u64	f_blocks;
	__u64	f_bfree;
	__u64	f_files;
	__u64	f_ffree;
	__u64	f_bavail;
	__kernel_fsid_t f_fsid;
	__u32	f_namelen;
	__u32	f_spare[6];
};

#endif
