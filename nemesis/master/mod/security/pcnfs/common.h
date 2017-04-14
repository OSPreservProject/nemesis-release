/* RE_SID: @(%)/tmp_mnt/vol/dosnfs/shades_SCCS/unix/pcnfsd/v2/src/SCCS/s.common.h 1.8 93/01/29 17:52:15 SMI */
/*
**=====================================================================
** Copyright (c) 1986-1993 by Sun Microsystems, Inc.
**
**         D I S C L A I M E R   S E C T I O N ,   E T C .
**
** pcnfsd is copyrighted software, but is freely licensed. This
** means that you are free to redistribute it, modify it, ship it
** in binary with your system, whatever, provided:
**
** - you leave the Sun copyright notice in the source code
** - you make clear what changes you have introduced and do
**   not represent them as being supported by Sun.
** - you do not charge money for the source code (unlikely, given
**   its free availability)
**
** If you make changes to this software, we ask that you do so in
** a way which allows you to build either the "standard" version or
** your custom version from a single source file. Test it, lint
** it (it won't lint 100%, very little does, and there are bugs in
** some versions of lint :-), and send it back to Sun via email
** so that we can roll it into the source base and redistribute
** it. We'll try to make sure your contributions are acknowledged
** in the source, but after all these years it's getting hard to
** remember who did what.
**
** The main contributors have been (in no special order):
**
** Glen Eustace <G.Eustace@massey.ac.nz>
**    user name caching for b-i-g password files
** Paul Emerson <paul@sdgsun.uucp>
**    cleaning up Interactive 386/ix handling, fixing the lp
**    interface, and generally tidying up the sources
** Keith Ericson <keithe@sail.labs.tek.com>
**    more 386/ix fixes
** Jeff Stearns <jeff@tc.fluke.com>
**    setuid/setgid for lpr
** Peter Van Campen <petervc@sci.kun.nl>
**    fixing setuid/gid stuff, syslog
** Ted Nolan <ted@usasoc.soc.mil>
**    /usr/adm/wtmp, other security suggestions
** Mogens Trab Damsgaard <trab@kubism.ku.dk>
**    allow SVR4 and BSD names to be used as appropriate.
** Arthur David Olson <ado@elsie.nci.nih.gov>
**    dozens of fixes all over the place
** Phil Servita <meister@ftp.com>
**    Ultrix authentication
** Greg Hinkel <guh@dsunx1.dsrd.ornl.gov>
**    more Ultrix stuff
** Nigel Metheringham <nigelm@ohm.york.ac.uk>
**    for the MIPS45x & NeXT port
** Edmund J. Sutcliffe <edmund@york.ac.uk>
**    for the Silicon Graphics IRIX port.
**
** Thanks to everyone who has contributed.
**
**    Geoff Arnold, PC-NFS architect <geoff@East.Sun.COM>
**=====================================================================
*/
/*
**=====================================================================
**             C U S T O M I Z A T I O N   S E C T I O N              *
**=====================================================================
*/

/*
**---------------------------------------------------------------------
** Define the following symbol to enable the use of a 
** shadow password file
**---------------------------------------------------------------------
**/

/* #define SHADOW_SUPPORT */

/*
**---------------------------------------------------------------------
** Define the following symbol to enable the logging 
** of authentication requests to /usr/adm/wtmp
**---------------------------------------------------------------------
**/

#define WTMP

/*
**------------------------------------------------------------------------
** Define the following symbol conform to Interactive
** System's 2.0
**------------------------------------------------------------------------
*/

/* #define ISC_2_0 */

/*
**---------------------------------------------------------------------
** Define the following symbol to use a cache of recently-used
** user names. This has certain uses in university and other settings
** where (1) the pasword file is very large, and (2) a group of users
** frequently logs in together using the same account (for example,
** a class userid).
**---------------------------------------------------------------------
*/

/* #define USER_CACHE */

/*
**---------------------------------------------------------------------
** Define the following symbol to build a System V version
**---------------------------------------------------------------------
*/

/* #define SYSV */

/*
**---------------------------------------------------------------------
** Define the following symbol to build an Ultrix 4.2 version
**---------------------------------------------------------------------
*/

/* #define ULTRIX */



/*
**---------------------------------------------------------------------
** Define the following symbol to build a version that
** will use the setusershell()/getusershell()/endusershell() calls
** to determine if a password entry contains a legal shell (and therefore
** identifies a user who may log in). The default is to check that
** the last two characters of the shell field are "sh", which should
** cope with "sh", "csh", "ksh", "bash".... See the routine get_password()
** in pcnfsd_misc.c for more details.
*/

/* #define USE_GETUSERSHELL */

/*
**---------------------------------------------------------------------
** Define the following symbol to build a version that
** will consult the NIS (formerly Yellow Pages) "auto.home" map to
** locate the user's home directory (returned by the V2 authentication
** procedure).
**---------------------------------------------------------------------
*/

/* #define USE_YP */


/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the way in which
** the "print file" printer operation is to be implemented.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_PRINT */
/* #define BSD_STYLE_PRINT */

/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the way in which
** the "list printers" printer operation is to be implemented.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_PR_LIST */
/* #define BSD_STYLE_PR_LIST */

/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the way in which
** the "list queue" printer operation is to be implemented.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_QUEUE */
/* #define BSD_STYLE_QUEUE */

/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the way in which
** the "cancel job" printer operation is to be implemented.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_CANCEL */
/* #define BSD_STYLE_CANCEL */

/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the way in which
** the "printer status" printer operation is to be implemented.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_STATUS */
/* #define BSD_STYLE_STATUS */

/*
**---------------------------------------------------------------------
** Define one of the following symbols to select the file or directory
** for which any change should cause the printer list to be rebuilt.
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define SVR4_STYLE_MONITOR */
/* #define BSD_STYLE_MONITOR */

/*
**---------------------------------------------------------------------
** trab -jan 1993:
** Define the following symbol, if you have defined BSD style print
** operations with:
**	BSD_STYLE_PRINT
**	BSD_STYLE_QUEUE
**	BSD_STYLE_STATUS
**	BSD_STYLE_MONITOR
** but have choosen:
**	SVR4_STYLE_PR_LIST 
** for "list printers".
** This will permit use of both the SVR4 printer names (from lpstat)
** and the BSD printer names (first name in printcap entries).
** See pcnfsd_printer.c for more details.
**---------------------------------------------------------------------
*/

/* #define ACCEPT_BSD_PR_NAMES */


/*
**=====================================================================
**                 P E R - O / S   S E C T I O N                      *
**                                                                    *
** For each supported OS, we gather the options in a single #ifdef    *
**=====================================================================
*/

#ifdef OSVER_SOL
#define BSD_STYLE_PRINT
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
/*  #define SVR4_STYLE_PR_LIST */
#define BSD_STYLE_PR_LIST
#define ACCEPT_BSD_PR_NAMES
#define LPR_COMMAND "/opt/lpd/bin/lpr -l"
#define LPRM_COMMAND "/opt/lpd/bin/lprm"
#define LPQ_COMMAND "/opt/lpd/bin/lpq"
#define LPC_COMMAND "/opt/lpd/bin/lpc"
#define SHADOW_SUPPORT
#define SVR4
#define WTMP
#endif

#ifdef OSVER_SOLARIS2X
#define SVR4_STYLE_PRINT
#define SVR4_STYLE_PR_LIST
#define SVR4_STYLE_QUEUE
#define SVR4_STYLE_CANCEL
#define SVR4_STYLE_STATUS
#define SVR4_STYLE_MONITOR
#define SHADOW_SUPPORT
#define SVR4
#define WTMP
#endif

#ifdef OSVER_SUNOS41
#define BSD_STYLE_PRINT
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define SVR4_STYLE_PR_LIST
#define ACCEPT_BSD_PR_NAMES
#define LPR_COMMAND "/usr/ucb/lpr -l"
#endif

#ifdef OSVER_SUNOS403C
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define SUNOS_403C
#define HAVE_NO_SIGACTION
#define HAVE_NO_STRSTR
#endif

#ifdef OSVER_BSD386
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define BSD386
#define LPR_COMMAND "/usr/bin/lpr"
#define LPRM_COMMAND "/usr/bin/lprm"
#define LPC_COMMAND "/usr/sbin/lpc"
#endif

#ifdef OSVER_ULTRIX
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define WTMP
#define ULTRIX
#define ULTRIX_AUTH
#endif

#ifdef OSVER_MIPS45X
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define SVR3_WTMP
#define WTMP
#define HAVE_NO_SIGACTION
#define HAVE_NO_STDLIB_H
#define HAVE_NO_STRSTR
#define LPR_COMMAND "/bsd43/bin/lpr"
#define LPRM_COMMAND "/bsd43/bin/lprm"
#define LPQ_COMMAND "/bsd43/bin/lpq"
#define LPC_COMMAND "/bsd43/bin/lpc"
#endif

#ifdef OSVER_IRIX4
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define SVR3_WTMP
#define WTMP
#define LPR_COMMAND "/usr/bsd/lpr -l"
#define LPRM_COMMAND "/usr/bsd/lprm"
#define LPQ_COMMAND "/usr/bsd/lpq"
#define LPC_COMMAND "/usr/etc/lpc"
#define SPOOLDIR "/usr/spool/pcnfs"
#endif

/* new OS entry for Irix 5.X modification, jan 94. alh@einoed.de */
/* patched up and fixed for IRIX 5.2 August '94 edmund@york.ac.uk */
#ifdef OSVER_IRIX5
#define WTMP
#define SVR4
#define SVR4_STYLE_PRINT
#define SVR4_STYLE_PR_LIST
#define SVR4_STYLE_QUEUE
#define SVR4_STYLE_CANCEL
#define SVR4_STYLE_STATUS
#define SVR4_STYLE_MONITOR
#endif

#ifdef OSVER_NeXT
#define BSD_STYLE_PRINT
#define BSD_STYLE_PR_LIST
#define BSD_STYLE_QUEUE
#define BSD_STYLE_CANCEL
#define BSD_STYLE_STATUS
#define BSD_STYLE_MONITOR
#define HAVE_NO_SIGACTION
#endif




/*
**=====================================================================
**              L O C A T I O N  O F   P R I N T   C O M M A N D S
**=====================================================================
*/
#ifndef LPR_COMMAND
#define LPR_COMMAND "/usr/ucb/lpr"
#endif  LPR_COMMAND

#ifndef LPRM_COMMAND
#define LPRM_COMMAND "/usr/ucb/lprm"
#endif  LPRM_COMMAND

#ifndef LPQ_COMMAND
#define LPQ_COMMAND "/usr/ucb/lpq"
#endif  LPQ_COMMAND

#ifndef LPC_COMMAND
#define LPC_COMMAND "/usr/etc/lpc"
#endif  LPC_COMMAND


/*
**=====================================================================
**              " O T H E R  S T U F F "   S E C T I O N              *
**=====================================================================
*/

#ifndef assert
#define assert(ex) {if (!(ex)) \
    {char asstmp[256];(void)sprintf(asstmp,"rpc.pcnfsd: Assertion failed: line %d of %s\n", \
    __LINE__, __FILE__); (void)msg_out(asstmp); \
    sleep (10); exit(1);}}
#endif
