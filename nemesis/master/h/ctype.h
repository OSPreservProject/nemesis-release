/*
 *	ctype.h
 *	-------
 *
 * $Id: ctype.h 1.2 Tue, 04 May 1999 18:45:38 +0100 dr10009 $
 */

#ifndef __ctype_h__
#define __ctype_h__

extern int isalnum	(int c);
extern int isalpha	(int c);
extern int isascii      (int c);
extern int iscntrl	(int c);
extern int isdigit	(int c);
extern int isgraph	(int c);
extern int islower	(int c);
extern int isprint	(int c);
extern int ispunct	(int c);
extern int isspace	(int c);
extern int isupper	(int c);
extern int isxdigit	(int c);

extern int toascii      (int c);
extern int toupper	(int c);
extern int tolower	(int c);

#endif /* __ctype_h__ */
