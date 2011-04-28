/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/* include/asm-arc/checksum.h */

#ifndef _ASM_ARC_CHECKSUM_H
#define _ASM_ARC_CHECKSUM_H

/* Sameer: So as to reach the definition for struct in6_addr */
#include <linux/in6.h>
/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
unsigned int csum_partial(const void * buff, int len, unsigned int sum);

/*
 * the same as csum_partial, but copies from src while it
 * checksums, and handles user-space pointer exceptions correctly, when needed.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

unsigned int csum_partial_copy(const char *src, char *dst, int len, int sum);

/*
 * the same as csum_partial_copy, but copies from user space.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

extern unsigned int csum_partial_copy_from_user(const char *src, char *dst,
						int len, int sum, int *csum_err);

#define csum_partial_copy_nocheck(src, dst, len, sum)	\
	csum_partial_copy((src), (dst), (len), (sum))

unsigned short ip_fast_csum(unsigned char *iph, unsigned int ihl);

#if 0
/* AmitS - This is done in arch/arcnommu/lib/checksum.c */

/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 */
static inline unsigned short ip_fast_csum(unsigned char * iph,
					  unsigned int ihl)
{
	unsigned int sum;
	unsigned char *tmp_iph = iph;

	__asm__ __volatile__ (
		/* Optimization: eliminate one nop by doing the sub
		 * before the load
		 */
		"sub.f  %2, %2, 4 \n"
		"ld     %0, [%1]  \n"
		"jle    2f \n"

		"mov    lp_count, 0x03 \n"
		"lp     1f           \n"
		"add    %3, %3, 4    \n"
		"add.f  %0, %0, [%3] \n"
		"1:                  \n"

                /* re-initialize tmp_iph to iph, so that we can use a
		 * loop construct
		 */
		"mov     %3, %1      \n"

		"1: \n"
		"add    %3, %3, 16   \n"
		"adc.f  %0, %0, [%3] \n"
		//"sub.f  \n"
		"sub    %3, %3, 12   \n"


		"2: \n"

		: "=r" (sum), "=r" (iph), "=r" (ihl), "=r" (tmp_iph)
		: "1" (iph), "2" (ihl), "3" (tmp_iph)
	);

	return 0;
}
#endif

/*
 *	Fold a partial checksum
 */

static inline __sum16 csum_fold(__wsum sum)
{

#if 0
	__asm__ (
		"add.f   %0, %0, %1     \n"
		"nop                    \n"
		"adc     %0, %0, 0xffff \n"
		: "=r" (sum)
		: "r"  (sum << 16), "0" (sum & 0xffff0000)
	);

	return (~sum) >> 16;
#endif
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

static inline unsigned long csum_tcpudp_nofold(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum)
{
	__asm__ __volatile__(
		"add.f %0, %0, %1   \n"
		"adc.f %0, %0, %2   \n"
		"adc.f %0, %0, %3   \n"
		"adc.f %0, %0, %4   \n"
		"adc   %0, %0, 0    \n"
		: "=r" (sum)
		: "r" (saddr), "r" (daddr), "r" (ntohs(len) << 16),
		  "r" (proto << 8), "0" (sum)
        : "cc"
	);

    return sum;
}

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
static inline unsigned short int csum_tcpudp_magic(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

#if 0
/* AmitS - This is done in arch/arcnommu/lib/checksum.c */
/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

static inline unsigned short ip_compute_csum(unsigned char * buff, int len)
{
    return csum_fold (csum_partial(buff, len, 0));
}
#endif

//#define _HAVE_ARCH_IPV6_CSUM
#if 0
static inline unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
						     struct in6_addr *daddr,
						     __u32 len,
						     unsigned short proto,
						     unsigned int sum)
{
	__asm__ (
		/* just clear the flags for the adc.f inst. in the loop */
		"sub.f 0, r0, r0      \n"
		/* Loop 4 times for the {sd}addr fields */
		"mov   lp_count, 0x04 \n"
		/* There have to be atleast 3 cycles betn loop count
		 * setup and last inst. in the loop
		 */
		"nop                  \n"
		"lp    1f             \n"
		"adc.f %0, %0, [%1]   \n"
		"add   %1, %1, 4      \n"
		"1:                   \n"
		"mov   lp_count, 0x04 \n"
		"nop                  \n"
		"lp    2f             \n"
		"adc.f %0, %0, [%2]   \n"
		"add   %2, %2, 4      \n"
		"2:                   \n"
		"adc.f %0, %0, %3     \n"
		"nop                  \n"
		"adc.f %0, %0, %4     \n"
		"nop                  \n"
		"adc.f %0, %0, 0x0    \n"
		: "=&r" (sum), "=r" (saddr), "=r" (daddr)
		: "r" (htonl(len)), "r" (htonl(proto)),
		  "0" (sum), "1" (saddr), "2" (daddr)
		);
	return csum_fold(sum);
}

#endif

#if 0
/*
 *	Copy and checksum to user
 */
#define HAVE_CSUM_COPY_USER
static inline unsigned int csum_and_copy_to_user(const char *src, char *dst,
				    int len, int sum, int *err_ptr)
{
	if (access_ok(VERIFY_WRITE, dst, len))
		return csum_partial_copy(src, dst, len, sum);

	if (len)
		*err_ptr = -EFAULT;

	return -1; /* invalid checksum */
}
#endif
#endif /* _ASM_ARC_CHECKSUM_H */
