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
 *  linux/include/asm-arc/socket.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_SOCKET_H
#define _ASM_ARC_SOCKET_H

#include <asm/sockios.h>

/* For setsockoptions(2) */
#define SOL_SOCKET	1

#define SO_DEBUG	1
#define SO_REUSEADDR	2
#define SO_TYPE		3
#define SO_ERROR	4
#define SO_DONTROUTE	5
#define SO_BROADCAST	6
#define SO_SNDBUF	7
#define SO_RCVBUF	8
#define SO_KEEPALIVE	9
#define SO_OOBINLINE	10
#define SO_NO_CHECK	11
#define SO_PRIORITY	12
#define SO_LINGER	13
#define SO_BSDCOMPAT	14
/* To add :#define SO_REUSEPORT 15 */
#define SO_PASSCRED	16
#define SO_PEERCRED	17
#define SO_RCVLOWAT	18
#define SO_SNDLOWAT	19
#define SO_RCVTIMEO	20
#define SO_SNDTIMEO	21

/* Security levels - as per NRL IPv6 - don't actually do anything */
#define SO_SECURITY_AUTHENTICATION		22
#define SO_SECURITY_ENCRYPTION_TRANSPORT	23
#define SO_SECURITY_ENCRYPTION_NETWORK		24

#define SO_BINDTODEVICE 25

/* Socket filtering */
#define SO_ATTACH_FILTER        26
#define SO_DETACH_FILTER        27

#define SO_PEERNAME             28
#define SO_TIMESTAMP		29
#define SCM_TIMESTAMP		SO_TIMESTAMP

#define SO_ACCEPTCONN		30

#define SO_SNDBUFFORCE	32
#define SO_RCVBUFFORCE	33
#define SO_PEERSEC	31
#define SO_PASSSEC	34
#define SO_TIMESTAMPNS 35
#define SCM_TIMESTAMPNS SO_TIMESTAMPNS

#define SO_MARK 36

/* Nast libc5 fixup - bletch */
#if defined(__KERNEL__)

/* Sameer: All defined in linux/net.h now */

/* Socket types. */
/* #define SOCK_STREAM	1		/\* stream (connection) socket	*\/ */
/* #define SOCK_DGRAM	2		/\* datagram (conn.less) socket	*\/ */
/* #define SOCK_RAW	3		/\* raw socket			*\/ */
/* #define SOCK_RDM	4		/\* reliably-delivered message	*\/ */
/* #define SOCK_SEQPACKET	5		/\* sequential packet socket	*\/ */
/* #define SOCK_PACKET	10		/\* linux specific way of	*\/ */
/* 					/\* getting packets at the dev	*\/ */
/* 					/\* level.  For writing rarp and	*\/ */
/* 					/\* other similar things on the	*\/ */
/* 					/\* user level.			*\/ */
/* #define	SOCK_MAX	(SOCK_PACKET+1) */
#endif

#endif /* _ASM_SOCKET_H */
