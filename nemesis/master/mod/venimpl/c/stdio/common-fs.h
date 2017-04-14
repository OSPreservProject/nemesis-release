/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      common-fs.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Prototypes for common filesystem emulation functions private to libc
** 
** ENVIRONMENT: 
**
**      Libc private functions
** 
** ID : $Id: common-fs.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef common_fs_h_
#define common_fs_h_

/* We assume that mounted filesystems have their offers listed in >mounts.
 * >fs is a local context, which will be created if it does not exist.
 * Pathnames have the form [fs:][/]name[/name...];
 * if a colon is present then everything up to it is interpreted as
 * a filesystem name.
 *
 * In >fs we store the following:
 *
 * client_name FSClient for filesystem "name"
 * cwd_name    FSDir for filesystem "name"
 * cwd         FSDir for currently selected filesystem
 *
 * We go for the filesystem 'init' if we don't have "cwd".
 *
 * XXX we need a story about which certificates we pass to the
 * filesystems.  Something should be passed to us from our parent in
 * the 'env' context. More later...
 * 
 */

/* Returns the FSDir for the current working directory */
extern FSDir_clp _fs_getcwd(void);

/* Returns the FSDir for the named filesystem */
extern FSDir_clp _fs_getfscwd(const char *fs);

#endif /* common_fs_h_ */
