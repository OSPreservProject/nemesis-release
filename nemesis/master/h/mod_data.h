/*
 *      h/mod_data.h
 *      ------------
 *
 * Dynamic module debugging support
 *
 * $Id: mod_data.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#ifndef _mod_data_h
#define _mod_data_h

typedef struct _mod_data_rec mod_data_rec;

/* 
 *  Each mod_data_entry contains the file name of a module or program,
 *  and the offset at which its symbol table should be loaded. A gdb
 *  script can munge this data to help automate the process of loading
 *  symbols 
 */

typedef struct _mod_data_entry {
    char *name;
    word_t offset;
} mod_data_entry;

struct _mod_data_rec {
    int num_entries;
    int max_entries;
    mod_data_entry *entries;
};

#endif
