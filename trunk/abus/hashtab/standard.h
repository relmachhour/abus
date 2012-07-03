/*
------------------------------------------------------------------------------
Standard definitions and types, Bob Jenkins
------------------------------------------------------------------------------
*/
#ifndef _STANDARD_H
#define _STANDARD_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef uint64_t  ub8;
#define UB8MAXVAL 0xffffffffffffffffLL
#define UB8BITS 64
typedef  int64_t  sb8;
#define SB8MAXVAL 0x7fffffffffffffffLL
typedef uint32_t  ub4;   /* unsigned 4-byte quantities */
#define UB4MAXVAL 0xffffffff
typedef  int32_t  sb4;
#define UB4BITS 32
#define SB4MAXVAL 0x7fffffff
typedef uint16_t  ub2;
#define UB2MAXVAL 0xffff
#define UB2BITS 16
typedef  int16_t  sb2;
#define SB2MAXVAL 0x7fff
typedef uint8_t   ub1;
#define UB1MAXVAL 0xff
#define UB1BITS 8
typedef  int8_t   sb1;   /* signed 1-byte quantities */
#define SB1MAXVAL 0x7f
typedef int       word;  /* fastest type available */

#ifndef align
# define align(a) (((size_t)a+(sizeof(void *)-1))&(~(sizeof(void *)-1)))
#endif /* align */
#define TRUE  1
#define FALSE 0
#define SUCCESS 0  /* 1 on VAX */

#endif /* _STANDARD_H */
