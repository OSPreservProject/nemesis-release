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
**      intmath.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Multiply and Divide for machines that don't have them.
**	On the arm only need divide
** 
** ID : $Id: intmath.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#define DIV 0
#define REM 1

#if 0
int __mulsi3(x, y)
     int x;
     int y;
{
     int a = 0;
     
     while (x)
     {
	  if (x & 1) a += y;
	  x >>= 1;
	  x &= 0x7fffffff;  /* >> sign extends */
	  y <<= 1;
     }
     return a;
}

unsigned long __umulsi3(x, y)
     unsigned long x;
     unsigned long y;
{
     unsigned long a = 0;
     
     while (x)
     {
	  if (x & 1) a += y;
	  x >>= 1;
	  x &= 0x7fffffff;  /* >> sign extends */
	  y <<= 1;
     }
     return a;
}

static int divremsi3(x, y, type)
     int x;
     int y;
     int type;
{
     unsigned int a = abs(x), b = abs(y);
     unsigned int res = 0, d = 1;
     
     int result, remainder;

     if (b > 0) while (b < a) b <<= 1, d <<= 1;
     
     do
     {
	  if ( a >= b ) a -= b, res += d;
	  b >>= 1;
	  d >>= 1;
     } while (d);
     
     result    = ( ((x ^ y) & (1<<31)) == 0) ? res : -(int)res;
     remainder = ( ( x & (1<<31) ) == 0)     ? a : -(int)a;
     
     return (type == DIV) ? result : remainder;
}


static unsigned long udivremsi3(a, b, type)
     unsigned long a;
     unsigned long b;
     int type;
{
     unsigned long res = 0, d = 1;
     unsigned long e = 1<<31;
     
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

int __divsi3(x, y)
     int x;
     int y;
{
     return divremsi3(x, y, DIV);
}

     
int __modsi3(x, y)
     int x;
     int y;
{
     return divremsi3(x, y, REM);
}

unsigned long __udivsi3(x, y)
     unsigned long x;
     unsigned long y;
{
     return udivremsi3(x, y, DIV);
}
     
unsigned long __umodsi3(x, y)
     unsigned long x;
     unsigned long y;
{
     return udivremsi3(x, y, REM);
}

	  
#endif /* 0 */
