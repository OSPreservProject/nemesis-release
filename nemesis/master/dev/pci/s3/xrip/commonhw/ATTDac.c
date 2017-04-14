/* $XConsortium: ATTDac.c,v 1.1 94/03/28 21:24:32 dpw Exp $ */
/*
 * Copyright 1994 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */


#include "compiler.h"

void xf86dactopel()
{
	(void)inb(0x3C8);
	return;
}

unsigned char 
xf86dactocomm()
{
	(void)inb(0x3C6);
	(void)inb(0x3C6);
	(void)inb(0x3C6);
	return(inb(0x3C6));
}

unsigned char
xf86getdaccomm()
{
	unsigned char ret;

	(void)xf86dactocomm();
	ret = inb(0x3C6);
	xf86dactopel();

	return(ret);
}

void
xf86setdaccomm(comm)
unsigned char comm;
{
	(void)xf86dactocomm();
	outb(0x3C6, comm);
	xf86dactopel();
	return;
}

void
xf86setdaccommbit(bits)
unsigned char bits;
{
	unsigned char tmp;

	tmp = xf86getdaccomm() | bits;
	xf86setdaccomm(tmp);
	return;
}

void
xf86clrdaccommbit(bits)
unsigned char bits;
{
	unsigned char tmp;

	tmp = xf86getdaccomm() & ~bits;
	xf86setdaccomm(tmp);
	return;
}
