/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      fopen.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      Libc
** 
** ID : $Id: fopen.c 1.2 Thu, 29 Apr 1999 09:58:07 +0100 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <exceptions.h>
#include <contexts.h>

#include <FSDir.ih>
#include <FileIO.ih>
#include <Rd.ih>
#include <Wr.ih>
#include <FSUtil.ih>

#include <exceptions.h>
#include <contexts.h>

#include "common-fs.h"

#define retifnull(x) \
if (! x )								\
{									\
    fprintf(stderr, "fopen: got passed a NULL `" #x "' pointer\n");	\
    return NULL;							\
}

static const char * const msg[] = {
    "<no error>",
    "No such file or directory",
    "File exists",
    "Permission denied",
    "unhandled exception"
};

FILE *fopen(const char *path, const char *mode)
{
    FSDir_clp			dir;
    FSTypes_Mode		omode;
    NOCLOBBER FSTypes_Options   options=0;
    bool_t			append;
    FileIO_clp			nemfile;
    FILE			*file;
    NOCLOBBER int		err;
    const char                  *colon;
    const char			*mp;
    FSTypes_RC			rc;
    FSUtil_clp                  fsutil;

    retifnull(mode);
    retifnull(path);

    if (*path == 0)
    {
	fprintf(stderr, "fopen: was passed zero-length pathname\n");
	return NULL;
    }

    TRY {
	fsutil = NAME_FIND("modules>FSUtil", FSUtil_clp);
    }
    CATCH_ALL {
	fprintf(stderr,"fopen: Can't find modules>FSUtil\n");
	fsutil = NULL;
    }
    ENDTRY;
    if (!fsutil) return NULL;

    mp = mode;

    /* Check to see if there's a colon in the path; if there is then
       get a directory for the appropriate filesystem, otherwise get
       the current directory */

    if ((colon=strchr(path,':'))!=NULL) {
       char                        fsys[64];

       strncpy(fsys, path, colon-path);
       fsys[colon-path]=0;

       /* printf("fopen: explicit filesystem `%s'\n",fsys); */

       dir=_fs_getfscwd(fsys);

       path = colon+1;
       
    } else {
       dir=_fs_getcwd();
    }

    if (!dir) {
       fprintf(stderr,"fopen: filesystem not present\n");
       return NULL;
    }

    /* Work out the options requested.  XXX the underlying options
     * available in Directory.if are not sufficiently powerful to
     * represent all these modes.  So some stange combinations won't
     * work too well. */

    switch(*mp)
    {
    case 'r':
	omode = FSTypes_Mode_Read;
	append = False;
	break;

    case 'w':
	omode = FSTypes_Mode_Write;
	append = False;
	break;

    case 'a':
	omode = FSTypes_Mode_Write;
	append = True;
	break;

    default:
	fprintf(stderr, "fopen: unknown mode `%.100s'\n", mode);
	return NULL;
    }

    /* consider the next character */
    mp++;

    if (*mp)
    {
	switch(*mp)
	{
	case 'b':
	    /* don't need to do anything special to handle binary :) */
	    /* sneakily peek at the next character and fall through if
	     * it isn't the end of the string */
	    mp++;
	    if (!*mp)
		break;
	    
	case '+':
	    omode = FSTypes_Mode_Write;
	    /* Not strictly true. Might want to specify "Create" too. */
	    mp++;
	    break;
	    
	default:
	    fprintf(stderr, "fopen: unknown mode `%.100s'\n", mode);
	    return NULL;
	}

	/* check for gunk */
	if (*mp && *mp != 'b')
	{
	    fprintf(stderr, "fopen: trailing garbage in mode spec `%.100s'\n",
		    mode);
	    return NULL;
	}
    }

    /* grab the memory for the 'FILE *' */
    file = Heap$Calloc(Pvs(heap), 1, sizeof(*file));
    if (!file)
    {
	fprintf(stderr, "fopen: out of memory!\n");
	return NULL;
    }

    err = 0;

    /* Lookup the file */
    if( (rc = FSDir$Lookup(dir, path, True)) != FSTypes_RC_OK) {

	/* 
	** SMH: if we're opening for read, or for append, then we require
	** the file to be already there, or else it's an error. 
	*/
	if(omode != FSTypes_Mode_Write) {

	    /* XXX 'Assume' file not found if not 'denied' [need proper rc] */
	    err = (rc == FSTypes_RC_DENIED) ? 3 : 1;
	    if (err!=1)
		fprintf(stderr,"fopen: %s while doing FSDir$Lookup "
			"on \"%s\"\n",msg[err], path);
	    Heap$Free(Pvs(heap), file);
	    return NULL;
	}
	
	/* SMH: we're opening for write: if denied => death, else => creat */
	if(rc == FSTypes_RC_DENIED) {
	    fprintf(stderr,"fopen: %s while doing FSDir$Lookup on \"%s\"\n",
		    msg[3], path);
	    Heap$Free(Pvs(heap), file);
	    return NULL;
	}

	rc = FSDir$Create(dir, path); 
	if(rc != FSTypes_RC_OK) {
	    /* XXX 'Assume' exception if not 'denied' [need proper rc] */
	    err = (rc == FSTypes_RC_DENIED) ? 3 : 4;  
	    fprintf(stderr,"fopen: %s while doing FSDir$Create of \"%s\"\n",
		    msg[err], path);
	    Heap$Free(Pvs(heap), file);
	    return NULL;
	} 

    }

    /* Here, we have a file to open - so open it! */
    rc = FSDir$Open(dir, 0, omode, options, &nemfile);
    switch (rc) {
    case FSTypes_RC_OK:
	break;
    case FSTypes_RC_DENIED:
	err = 3;
	break;
    case FSTypes_RC_FAILURE:
	err = 4;
	break;
    }
    if (!nemfile || err) {
	fprintf(stderr,"fopen: %s while doing FSDir$Open on \"%s\"\n",
		msg[err], path);
	FSDir$Release(dir);
	Heap$Free(Pvs(heap), file);
	return NULL;
    }

    /* note the File, so we can Dispose() it later */
    file->file = nemfile;

    /* Get a reader on the file */
    if (omode == FSTypes_Mode_Read) {
	file->rd = FSUtil$GetRd(fsutil, nemfile, Pvs(heap), True);
    } else {
	/* If we're writing, get a reader and a writer */
	file->rd = FSUtil$GetRdWr(fsutil, nemfile, Pvs(heap), True,
				  &file->wr);

	/* If we're appending, seek to the end of the file */
	if (append) {
	    Wr$Seek(file->wr, -1);
	}
    }

    file->error = False;

    /* That's all! */
    return file;
}


/* End of fopen.c */
