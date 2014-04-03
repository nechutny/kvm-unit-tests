/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright IBM Corp. 2008
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#ifndef __LIBCFLAT_H
#define __LIBCFLAT_H

#include <stdarg.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned u32;
typedef signed s32;
typedef unsigned long ulong;
typedef unsigned long long u64;
typedef signed long long s64;
typedef unsigned long size_t;
typedef _Bool bool;

#define true 1
#define false 0

extern void halt(int code);
extern void exit(int code);

extern unsigned long strlen(const char *buf);
extern char *strcat(char *dest, const char *src);
extern int strcmp(const char *a, const char *b);
extern char *strchr(const char *s, int c);

extern int printf(const char *fmt, ...);
extern int vsnprintf(char *buf, int size, const char *fmt, va_list va);

extern void puts(const char *s);

extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern void *memchr(const void *s, int c, size_t n);

extern long atol(const char *ptr);
#define ARRAY_SIZE(_a)  (sizeof(_a)/sizeof((_a)[0]))

#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
#define container_of(ptr, type, member) ({				\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})

#define __unused __attribute__((__unused__))
#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)
#define NULL ((void *)0UL)
#include "errno.h"
#endif
