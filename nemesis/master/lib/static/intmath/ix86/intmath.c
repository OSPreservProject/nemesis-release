/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      lib/c/intmath/ix86/intmath.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Multiply and Divide for machines that don't have them.
**	ix86 version - 64-bit div and rem only.
**	Horribly slow, but there you go.
** 
** ID : $Id: intmath.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
**
*/

/* 
 *  GCC for mips_ultrix_4.3 generates calls to the following
 *  routines:
 * 
 *  __divdi3
 *  __moddi3
 *  __udivdi3
 *  __umoddi3
 */

#define DIV 0
#define REM 1


/* 
 * 64-bit routines (long long on mips)
 */

static long long divremdi3(
     long long x,
     long long y,
     int type)
{
     unsigned long long a = (x < 0) ? -x : x;
     unsigned long long b = (y < 0) ? -y : y;
     unsigned long long res = 0, d = 1;
     
     long long result, remainder;

     if (b > 0) while (b < a) b <<= 1, d <<= 1;
     
     do
     {
	  if ( a >= b ) a -= b, res += d;
	  b >>= 1;
	  d >>= 1;
     } while (d);
     
     result    = ( ((x ^ y) & (1ll<<63)) == 0) ? res : -(long long)res;
     remainder = ( ( x & (1ll<<63) ) == 0)     ? a : -(long long)a;
     
     return (type == DIV) ? result : remainder;
}


static unsigned long long udivremdi3(
     unsigned long long a,
     unsigned long long b,
     int type)
{
     unsigned long long res = 0, d = 1;
     unsigned long long e = 1ll<<63;
     
     if (a == 0) return (0);

     while ((a & e) == 0) 
	e >>= 1;

     if (b > 0) while (b < e) b <<= 1, d <<= 1;
     
     do
     {
	  if (a >= b) a -= b, res += d;
	  b >>= 1;
	  d >>= 1;
     } while (d);
     
     return (type == DIV) ? res : a;
}

long long __divdi3(long long x, long long y)
{
     return divremdi3(x, y, DIV);
}

unsigned long long __udivdi3(unsigned long long x, unsigned long long y)
{
     return udivremdi3(x, y, DIV);
}

     
long long __moddi3(long long x, long long y)
{
     return divremdi3(x, y, REM);
}

unsigned long long __umoddi3(unsigned long long x, unsigned long long y)
{
     return udivremdi3(x, y, REM);
}

/* ASSUME LITTLE-ENDIAN */

typedef union {
  struct {
    unsigned long low;
    unsigned long high;
  } s;
  long long ll;
} u64;

unsigned long __ucmpdi2 (unsigned long long x, unsigned long long y)
{
  u64 xu, yu;

  xu.ll = x;
  yu.ll = y;

  if (xu.s.high < yu.s.high)
    return 0;
  else if (xu.s.high > yu.s.high)
    return 2;

  if (xu.s.low < yu.s.low)
    return 0;
  else if (xu.s.low > yu.s.low)
    return 2;

  return 1;
}
