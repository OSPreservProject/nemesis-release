/*
 *      ntsc/ix86/string.h
 *      ------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * Very simple string handling functions for GDB stubs
 */

#ifndef STRING_H
#define STRING_H

extern char *strcpy(char *dest, const char *src);
extern int strlen(const char *string);
extern void memset(void *address, unsigned int value, int len);
extern void * memcpy(void * dest,const void *src,int count);
extern int memcmp(const void * cs,const void * ct,int count);
extern long simple_strtoul(const char *c, char **endp, int base);

#endif /* STRING_H */
