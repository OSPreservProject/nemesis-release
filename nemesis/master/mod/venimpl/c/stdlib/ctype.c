/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 *
 *	ctype.c
 *	-------
 *
 * $Id: ctype.c 1.2 Tue, 04 May 1999 18:45:38 +0100 dr10009 $
 */

/*
 * We use a denser than normal coding by observing that _C _P
 * _N _U and _L are distinct and so can be implemented by a value
 * rather than a bit. This means we can use 5 bits each instead of 9.
 * The only problem then is that ctrl is not true for non ascii values
 * so we use the top bit for detecting it when necessary even though
 * its lower bits are important for distinguishing it from others.
 * This leads to a six bit implementation.
 */

#define _C	040
#define _P	001
#define _N	002
#define _U	004
#define _L	005

#define _S	010
#define _B	020
#define _X	010

static const unsigned char ctype[1+256] = {
    0,
    _C,		_C,	_C,	_C,	_C,	_C,	_C,	_C,
    _C,	      _C|_S|_B,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C,	_C,
    _C,		_C,	_C,	_C,	_C,	_C,	_C,	_C,
    _C,		_C,	_C,	_C,	_C,	_C,	_C,	_C,
    _S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
    _P,		_P,	_P,	_P,	_P,	_P,	_P,	_P,
    _N,		_N,	_N,	_N,	_N,	_N,	_N,	_N,
    _N,		_N,	_P,	_P,	_P,	_P,	_P,	_P,
    _P,		_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
    _U,		_U,	_U,	_U,	_U,	_U,	_U,	_U,
    _U,		_U,	_U,	_U,	_U,	_U,	_U,	_U,
    _U,		_U,	_U,	_P,	_P,	_P,	_P,	_P,
    _P,		_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
    _L,		_L,	_L,	_L,	_L,	_L,	_L,	_L,
    _L,		_L,	_L,	_L,	_L,	_L,	_L,	_L,
    _L,		_L,	_L,	_P,	_P,	_P,	_P,	_C,
};

int isalnum(int c)
{
    return ctype[c+1] & 006;
}
int isalpha(int c)
{
    return ctype[c+1] & 004;
}
int isascii(int c)
{
  return ((c & 0x80) == 0);  // Is this a 7 bit only value
}
int iscntrl(int c)
{
    return ctype[c+1] & 040;
}
int isdigit(int c)
{
    return ctype[c+1] & 002;
}
int isgraph(int c)
{
    return ctype[c+1] & 007;
}
int islower(int c)
{
    return (ctype[c+1] & 007) == 005;
}
int isprint(int c)
{
    return ctype[c+1] & 027;
}
int ispunct(int c)
{
    return (ctype[c+1] & 007) == 001;
}
int isspace(int c)
{
    return (ctype[c+1] & 014) == 010;
}
int isupper(int c)
{
    return (ctype[c+1] & 007) == 004;
}
int isxdigit(int c)
{
    return (ctype[c+1] & 002) || ((ctype[c+1] & 014) == 014);
}
int toascii(int c)
{
  return (c & 0x7F);
}
int tolower(int c)
{
    if ((c >= 'A') && (c <= 'Z'))
	return c + 'a' -'A';
    else
	return c;
}
int toupper(int c)
{
    if ((c >= 'a') && (c <= 'z'))
	return c + 'A' - 'a';
    else
	return c;
}

/* End of ctype.c */
