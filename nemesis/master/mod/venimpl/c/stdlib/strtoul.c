/*  -*- Mode: C;  -*-
 * File: strtoul.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert NPTR to a `unsigned long int' in base BASE.
 **           If BASE is 0 the base is determined by the presence of a leading
 **           zero, indicating octal or a leading "0x" or "0X", indicating 
 **           hexadecimal.
 **           If BASE is < 2 or > 36, it is reset to 10.
 **           If ENDPTR is not NULL, a pointer to the character after the last
 **           one converted is stored in *ENDPTR.
 **
 **           Entry at Nemesis Level RAISES stdlib_uoflow. 
 **
 **
 ** $Id: strtoul.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
 */
/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */


#define	UNSIGNED	1

#include "strtol.c"

/*
 * end stroul.c
 */
