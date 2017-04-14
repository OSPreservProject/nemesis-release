/*
 * HISTORY
 */

/*
 * Definitions for nemboot program
 */
#define PAGSIZ          (1<<13)         /* page size for today          */

struct neminfo {
	long    tsize;          /* text size in bytes, padded to DW bdry*/
	long    dsize;          /* initialized data "  "                */
	long    bsize;          /* uninitialized data "   "             */
	long    entry;          /* entry 				*/
	long    text_start;     /* base of text used for this file      */
	long    data_start;     /* base of data used for this file      */
	long    bss_start;      /* base of bss used for this file       */
	long	image;		/* pointer to the image in memory	*/
};


