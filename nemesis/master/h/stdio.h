/*
 *	stdio.h
 *	-------
 *
 * $Id: stdio.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#ifndef __stdio_h__
#define __stdio_h__

/*
 * ANSI Conformatory section - Not all of these are implemented
 */

#include <nemesis.h>		/* Needed for .ih files */
#include <stddef.h>
#include <stdarg.h>

#include <Wr.ih>
#include <Rd.ih>
#include <FileIO.ih>

typedef struct _file {
    Rd_clp	rd;
    Wr_clp	wr;
    FileIO_clp	file;
    bool_t	error;
} FILE;

static INLINE
int feof(FILE *foo) {
    if (foo->rd)     return Rd$EOF(foo->rd);
    return 0;
}
#define stdin	((FILE *) &Pvs(in))	/* XXX - temp. HACK */
#define stdout	((FILE *) &Pvs(in))	/* XXX - temp. HACK */
#define stderr  ((FILE *) &Pvs(out))	/* XXX - temp. HACK */

typedef long	fpos_t;

#define _IOFBF barf_for_now
#define _IOLBF barf_for_now
#define _IONBF barf_for_now

#define BUFSIZ barf_for_now

#define EOF (-1)

#define FOPEN_MAX barf_for_now

#define FILENAME_MAX barf_for_now

#define L_tmpnam barf_for_now

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ------ 4.9.4	Operations on Files				*/
extern int	 remove		(const char *filename);
extern int	 rename		(const char *oldname, const char *newname);
extern FILE	*tmpfile	(void);
extern char	*tmpnam		(char *s);

/* ------ 4.9.5	File Access Functions				*/
extern int	 fclose		(FILE *stream);
extern int	 fflush		(FILE *stream);
extern FILE	*fopen		(const char *filename, const char *mode);
extern FILE	*freopen	(const char *filename, const char *mode,
				 FILE *stream);
extern void	 setbuf		(FILE *stream, char *buf);
extern int	 setvbuf	(FILE *stream, char *buf, int mode,
				 size_t size);

/* ------ 4.9.6	Formatted Input/Ouput Functions			*/
extern int	 fprintf	(FILE *stream, const char *format, ...);
extern int	 fscanf		(FILE *stream, const char *format, ...);
extern int	 libc_printf	(const char *format, ...);
#define  printf(_format, args...)     libc_printf (_format, ## args )
extern int	 scanf		(const char *format, ...);
extern int	 sprintf	(char *s, const char *format, ...);
extern int	 sscanf		(const char *s, const char *format, ...);
extern int	 vfprintf	(FILE *stream, const char *format,
				 va_list arg);
extern int	 vprintf	(const char *format, va_list arg);
extern int	 vsprintf	(char *s, const char *format, va_list arg);

/* ------ 4.9.7	Character Input/Ouput Functions			*/
extern int	 fgetc		(FILE *stream);
extern char	*fgets		(char *s, int n, FILE *stream);
extern int	 fputc		(int c, FILE *stream);
extern int	 fputs		(const char *s, FILE *stream);
extern int	 getc		(FILE *stream);
extern int	 getchar	(void);
extern char	*gets		(char *s);
extern int	 putc		(int c, FILE *stream);
extern int	 putchar	(int c);
extern int	 puts		(const char *s);
extern int	 ungetc		(int c, FILE *stream);

/* ------ 4.9.8	Direct Input/Ouput Functions			*/
extern size_t	 fread		(void *ptr, size_t sizeofPtr, size_t nmemb,
				 FILE *stream);
extern size_t	 fwrite		(const void *ptr, size_t size, size_t nmemb,
				 FILE *stream);

/* ------ 4.9.9	File Positioning Functions			*/
extern int	 fgetpos	(FILE *stream, fpos_t *pos);
extern int	 fseek		(FILE *stream, long int offset, int whence);
extern int	 fsetpos	(FILE *stream, const fpos_t *pos);
extern long	 ftell		(FILE *stream);
extern void	 rewind		(FILE *stream);

/* ----- 4.9.10	Error-Handling Functions			*/
extern void	 clearerr	(FILE *stream);
extern int	 feof		(FILE *stream);
extern int	 ferror		(FILE *stream);
extern void	 perror		(const char *s);

/* ----- */
/*
 * RJB Section
 */

#if defined(__GNUC__) && !defined(__PEDANTIC_ANSI__)
extern int eprintf(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern int wprintf(Wr_cl *stream, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
#else
extern int eprintf(const char *format, ...);
extern int wprintf(Wr_cl *stream, const char *format, ...);
#endif

#define putc fputc

#endif /* __stdio_h__ */
