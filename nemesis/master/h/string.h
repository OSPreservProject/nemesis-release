/*  -*- Mode: C;  -*-
 * File: string.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library
 **
 ** FUNCTION: String and mem.. header.
 **
 ** HISTORY:
 ** Created: Thu Apr 21 14:11:35 1994 (jch1003)
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __string_h__
#define __string_h__

#include <nemesis.h>
#include <stddef.h>

#include <Heap.ih>

/*
 *  prototypes
 */


extern char
        *strcat( char *s1, const char *s2 ),	
	*strchr( const char *s, int c ),
	*strcpy( char *s1, const char *s2 ),
	*strdup( const char *s ),
	*strduph( const char *s, Heap_clp h ),
	*strerror( int n ),		
	*strncat( char *s1, const char *s2, size_t n ),
	*strncpy( char *s1, const char *s2, size_t n ),
	*strpbrk(const char *s1, const char *s2 ),	
	*strrchr( const char *s, int c ),
	*strstr( const char *s, const char *t ),
	*strtok( char *s1, const char *s2 );

extern int
	memcmp( const void *s1, const void *s2, size_t n ),
	strcmp( const char *s1, const char *s2 ),
	strcasecmp( const char *s1, const char *s2),
	strcoll( const char *s1, const char *s2 ),
	strncmp( const char *s1, const char *s2, size_t n );

extern void
	*memchr( const void *s, int c, size_t n ),	
	*memcpy( void *s1, const void *s2, size_t n ),
	*memmove( void *s1, const void *s2, size_t n ),
	*memset( void *s, int c, size_t n );

extern size_t
	strcspn( const char *s1, const char *s2 ),
	strlen( const char *s ),
	strspn( const char *s1, const char *s2 );

#endif /* __string_h__ */


/*
 * end string.h
 */
