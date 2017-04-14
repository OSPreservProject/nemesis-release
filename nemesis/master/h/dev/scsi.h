/*
*****************************************************************************
**                                                                          *
**  Copyright 1993, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**	h/dev/scsi.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      SCSI command definitions
** 
** ENVIRONMENT: 
**
**      Nemesis Includes
** 
** ID : $Id: scsi.h 1.1 Tue, 04 May 1999 18:46:07 +0100 dr10009 $
** 
*/

/*  -*- Mode: C;  -*-
 * File: scsi.h
 * Author: Sai Lai Lo (sll@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: SCSI driver
 **
 ** FUNCTION: 
 **
 ** HISTORY:
 ** Created: 1 Dec 1992 (approx.) (sll)
 ** Last Edited: Fri Jan 21 17:43:26 1994 By Sai Lai Lo
 $Log: scsi.h,v $
 Revision 1.3  1997/04/06 21:02:25  smh22
 more defns (from ncr)

 Revision 1.2  1996/09/23 12:27:11  smh22
 see updates

 * Revision 1.3  1994/01/24  13:11:13  sll
 * Various updates.
 *
 * Revision 1.3  1994/01/24  13:11:13  sll
 * Various updates.
 *
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __SCSI_H__
#define __SCSI_H__

/*
 * SCSI command format
 */

/* WARNING: THESE ARE ENDIAN DEPENDENT TYPE DECLARATIONS.*/

#ifdef BIG_ENDIAN
union scsi_cmd
{

	struct scsi_generic
	{
		uchar_t	opcode;
		uchar_t	lun:3;
		uchar_t  :5;
		uchar_t	bytes[10];
	} generic;

	struct scsi_test_unit_ready
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t  :5;
		uchar_t	unused[3];
		uchar_t  :3;
		uchar_t  flag:4;
		uchar_t	link:1;
	} test_unit_ready;

	struct scsi_send_diag
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t	pf:1;
		uchar_t	:1;
		uchar_t	selftest:1;
		uchar_t	dol:1;
		uchar_t	uol:1;
		uchar_t	unused[1];
		uchar_t	paramlen[2];
		uchar_t  :3;
		uchar_t  flag:4;
		uchar_t	link:1;
	} send_diag;

	struct scsi_sense
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t  :5;
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t  :6;
		uchar_t  flag:1;
		uchar_t	link:1;
	} sense;

	struct scsi_inquiry
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t  :5;
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t  :6;
		uchar_t  flag:1;
		uchar_t	link:1;
	} inquiry;

	struct scsi_mode_sense
	{
		uchar_t	op_code;
		uchar_t	lun:3;	
		uchar_t	rsvd:1;
		uchar_t	dbd:1;
		uchar_t	:3;
		uchar_t	page_ctrl:2;
		uchar_t	page_code:6;
		uchar_t	unused;
		uchar_t	length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} mode_sense;

	struct scsi_mode_select
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t  pf:1
		uchar_t  :3;
		uchar_t  sp:1
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} mode_select;

	struct scsi_reassign_blocks
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t  :5;
		uchar_t	unused[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} reassign_blocks;

	struct scsi_rw
	{
		uchar_t	op_code;
		uchar_t	lun:3;
		uchar_t	addr_2:5;	/* Most significant */
		uchar_t	addr_1;
		uchar_t	addr_0;		/* least significant */
		uchar_t	length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} rw;

	struct scsi_rw_big
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t  :4;
		uchar_t	rel_addr:1;
		uchar_t	addr_3;         /* Most significant */
		uchar_t	addr_2;
		uchar_t	addr_1;
		uchar_t	addr_0;		/* least significant */
		uchar_t	reserved;;
		uchar_t	length2;
		uchar_t	length1;
		uchar_t  vendor:2;
		uchar_t  :4;
		uchar_t  flag:1;
		uchar_t	link:1;
	} rw_big;

	struct scsi_rw_tape
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t  :4;
		uchar_t	fixed:1;
		uchar_t	len[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} rw_tape;

	struct scsi_format
	  {
	        uchar_t op_code;
		uchar_t lun:3;
		uchar_t fmtdat:1;
		uchar_t cmplst:1;
		uchar_t dlf:3;
		uchar_t pattern;
		uchar_t interleave1;
		uchar_t interleave0;
		uchar_t vendor:2;
		uchar_t reserved:4;
		uchar_t flag:1;
		uchar_t link:1;
	  } format;

	struct scsi_read_capacity
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	addr_3;	/* Most Significant */
		uchar_t	addr_2;
		uchar_t	addr_1;
		uchar_t	addr_0;	/* Least Significant */
		uchar_t	unused[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} read_capacity;

	struct scsi_start_stop
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	unused[2];
		uchar_t  :7;
		uchar_t	start:1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} start_stop;

	struct scsi_space
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t  :3;
		uchar_t	code:2;
		uchar_t	number[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} space;
#define SP_BLKS	0
#define SP_FILEMARKS 1
#define SP_SEQ_FILEMARKS 2
#define	SP_EOM	3

	struct scsi_write_filemarks
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	number[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} write_filemarks;

	struct scsi_rewind
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t  :4;
		uchar_t	immed:1;
		uchar_t	unused[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} rewind;

	struct scsi_load
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t  :4;
		uchar_t	immed:1;
		uchar_t	unused[2];
		uchar_t  :6;
		uchar_t  reten:1;
		uchar_t	load:1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} load;
#define LD_UNLOAD 0
#define LD_LOAD 1

	struct scsi_reserve
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} reserve;

	struct scsi_release
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} release;

	struct scsi_prevent
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	unused[2];
		uchar_t  :7;
		uchar_t	prevent:1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} prevent;
#define	PR_PREVENT 1
#define PR_ALLOW   0

	struct scsi_blk_limits
	{
		uchar_t	op_code;
		uchar_t  lun:3;
		uchar_t	:5;
		uchar_t	unused[3];
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	} blk_limits;


	struct scsi_set_window
	  {
	        uchar_t  op_code;
		uchar_t  lun:3;
		uchar_t  :5;
		uchar_t  unused[4];
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	   } set_window;

	struct scsi_get_window
	  {
	        uchar_t  op_code;
		uchar_t  lun:3;
		uchar_t  :4;
		uchar_t  single:1;
		uchar_t  lun:3;
		uchar_t  unused[3];
		uchar_t  win_id;
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	  } get_window;

	struct scsi_scan
	  {
	        uchar_t  op_code;
		uchar_t  lun:3;
		uchar_t  :5;
		uchar_t  unused[2];
		uchar_t  length;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	  } scan;

	struct scsi_scan_read
	  {
	        uchar_t  op_code;
		uchar_t  lun:3;
		uchar_t  :5;
		uchar_t  datatype;
		uchar_t  unused;
		uchar_t  dataq2;
	        uchar_t  dataq1;
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	  } scan_read;

	struct scsi_get_data_buffer_status
	  {
	        uchar_t  op_code;
		uchar_t  lun:3;
		uchar_t  :5;
		uchar_t  unused[5];
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	:6;
		uchar_t	flag:1;
		uchar_t	link:1;
	  } scan_getbufstatus;

};

union mode_sel_data
{
	struct
	{
		uchar_t	unused[2];
		uchar_t  :1;
		uchar_t  buf_mode:3;
		uchar_t	speed:4;
		uchar_t	length;
		uchar_t	density;
		uchar_t	seg_len[3];
		uchar_t	unused2;
		uchar_t	blklen[3];
	}tape;
};

struct scsi_device_inquiry
{
	uchar_t	device_qualifier:3;
	uchar_t	device_type:5;
	uchar_t	removable:1;
	uchar_t	dev_qual2:7;
	uchar_t  iso:2;
	uchar_t  ecma:3;
	uchar_t	ansi:3;
	uchar_t  aenc:1;
	uchar_t  trmiop:1;
	uchar_t  res1:2;
	uchar_t	response_format:4;
	uchar_t	additional_length;
	uchar_t	unused[2];
	uchar_t  reladr:1;
	uchar_t  wbus32:1;
	uchar_t  wbus16:1;
	uchar_t  sync:1;
	uchar_t	link:1;
	uchar_t  res2:1;
	uchar_t  cmdq:1;
	uchar_t  sftre:1;
	char	vendor[8];
	char	product[16];
	char	revision[4];
	uchar_t	extra[8];
};

struct	scsi_sense_data
{
  union
    {
      struct
	{
	  uchar_t  valid:1;
	  uchar_t  error_class:3;
	  uchar_t  error_code:4;
	  uchar_t  vendor:3;
	  uchar_t	blockhi:5;
	  uchar_t	blockmed;
	  uchar_t	blocklow;
	} unextended;
      struct
	{
	  uchar_t  valid:1;
	  uchar_t  error_class:3;
	  uchar_t  error_code:4;
	  uchar_t  segment;
	  uchar_t	filemark:1;
	  uchar_t	eom:1;
	  uchar_t	ili:1;
	  uchar_t	res:1;
	  uchar_t	sense_key:4;
	  uchar_t	info[4];
	  uchar_t	extra_len;
	  /* allocate enough room to hold new stuff
	     uchar_t	cmd_spec_info[4];
	     uchar_t	add_sense_code;
	     uchar_t	add_sense_code_qual;
	     uchar_t	fru;
	     uchar_t	sksv:1;
	     uchar_t	sense_key_spec_1:7;
	     uchar_t	sense_key_spec_2;
	     uchar_t	sense_key_spec_3;
	     ( by increasing 16 to 26 below) */
	  uchar_t	extra_bytes[26];
	} extended;
    }ext;
};

struct	scsi_sense_data_new
{
  union
    {
      struct
	{
	  uchar_t	valid:1;
	  uchar_t	error_code:7;
	  uchar_t	vendor:3;
	  uchar_t	blockhi:5;
	  uchar_t	blockmed;
	  uchar_t	blocklow;
	} unextended;
      struct
	{
	  uchar_t	valid:1;
	  uchar_t	error_code:7;
	  uchar_t	segment;
	  uchar_t	filemark:1;
	  uchar_t	eom:1;
	  uchar_t	ili:1;
	  uchar_t	:1;
	  uchar_t	sense_key:4;
	  uchar_t	info[4];
	  uchar_t	extra_len;
	  uchar_t	cmd_spec_info[4];
	  uchar_t	add_sense_code;
	  uchar_t	add_sense_code_qual;
	  uchar_t	fru;
	  uchar_t	sksv:1;
	  uchar_t	sense_key_spec_1:7;
	  uchar_t	sense_key_spec_2;
	  uchar_t	sense_key_spec_3;
	  uchar_t	extra_bytes[16];
	} extended;
    }ext;
};

struct scsi_blk_limits_data
{
	uchar_t	reserved;
	uchar_t	max_length_2;	/* Most significant */
	uchar_t	max_length_1;
	uchar_t	max_length_0;	/* Least significant */
	uchar_t	min_length_1;	/* Most significant */
	uchar_t	min_length_0;	/* Least significant */
};

struct scsi_read_cap_data
{
	uchar_t	addr_3;	/* Most significant */
	uchar_t	addr_2;
	uchar_t	addr_1;
	uchar_t	addr_0;	/* Least significant */
	uchar_t	length_3;	/* Most significant */
	uchar_t	length_2;
	uchar_t	length_1;
	uchar_t	length_0;	/* Least significant */
};

struct scsi_reassign_blocks_data
{
	uchar_t	reserved[2];
	uchar_t	length_msb;
	uchar_t	length_lsb;
	uchar_t	dlbaddr_3;	/* defect logical block address (MSB) */
	uchar_t	dlbaddr_2;
	uchar_t	dlbaddr_1;
	uchar_t	dlbaddr_0;	/* defect logical block address (LSB) */
};

struct scsi_mode_sense_data
{
  union
  {
    struct pgcode_3 
    {
      uchar_t	data_length;	/* Sense data length */
      uchar_t	unused1[3];
      uchar_t	density;
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;
      uchar_t :2;		
      uchar_t pg_code:6;	/* page code (should be 3)	      */
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t trk_z_1;	/* tracks per zone (MSB)	      */
      uchar_t trk_z_0;	/* tracks per zone (LSB)	      */
      uchar_t alt_sec_1;	/* alternate sectors per zone (MSB)   */
      uchar_t alt_sec_0;	/* alternate sectors per zone (LSB)   */
      uchar_t alt_trk_z_1;	/* alternate tracks per zone (MSB)    */
      uchar_t alt_trk_z_0;	/* alternate tracks per zone (LSB)    */
      uchar_t alt_trk_v_1;	/* alternate tracks per volume (MSB)  */
      uchar_t alt_trk_v_0;	/* alternate tracks per volume (LSB)  */
      uchar_t ph_sec_t_1;	/* physical sectors per track (MSB)   */
      uchar_t ph_sec_t_0;	/* physical sectors per track (LSB)   */
      uchar_t bytes_s_1;	/* bytes per sector (MSB)	      */
      uchar_t bytes_s_0;	/* bytes per sector (LSB)	      */
      uchar_t interleave_1;/* interleave (MSB)		      */
      uchar_t interleave_0;/* interleave (LSB)		      */
      uchar_t trk_skew_1;	/* track skew factor (MSB)	      */
      uchar_t trk_skew_0;	/* track skew factor (LSB)	      */
      uchar_t cyl_skew_1;	/* cylinder skew (MSB)		      */
      uchar_t cyl_skew_0;	/* cylinder skew (LSB)		      */
      uchar_t ssec:1;
      uchar_t hsec:1;
      uchar_t rmb:1;
      uchar_t surf:1;
      uchar_t reserved1:4;
      uchar_t reserved2;
      uchar_t reserved3;
      uchar_t reserved4;
    } pgcode_3;
    struct pgcode_4 
    {
      uchar_t	data_length;	/* Sense data length */
      uchar_t	unused1[3];
      uchar_t	density;
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;
      uchar_t mbone:1;	/* must be one			      */
      uchar_t pg_code:7;	/* page code (should be 4)	      */
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t ncyl_2;	/* number of cylinders (MSB)	      */
      uchar_t ncyl_1;	/* number of cylinders 		      */
      uchar_t ncyl_0;	/* number of cylinders (LSB)	      */
      uchar_t nheads;	/* number of heads 		      */
      uchar_t st_cyl_wp_2;	/* starting cyl., write precomp (MSB) */
      uchar_t st_cyl_wp_1;	/* starting cyl., write precomp	      */
      uchar_t st_cyl_wp_0;	/* starting cyl., write precomp (LSB) */
      uchar_t st_cyl_rwc_2;/* starting cyl., red. write cur (MSB)*/
      uchar_t st_cyl_rwc_1;/* starting cyl., red. write cur      */
      uchar_t st_cyl_rwc_0;/* starting cyl., red. write cur (LSB)*/
      uchar_t driv_step_1;	/* drive step rate (MSB)	      */
      uchar_t driv_step_0;	/* drive step rate (LSB)	      */
      uchar_t land_zone_2;	/* landing zone cylinder (MSB)	      */
      uchar_t land_zone_1;	/* landing zone cylinder 	      */
      uchar_t land_zone_0;	/* landing zone cylinder (LSB)	      */
      uchar_t reserved1:6;
      uchar_t rpl:2;
      uchar_t rot_offset;
      uchar_t reserved2;
      uchar_t rot_rate_1;
      uchar_t rot_rate_0;
      uchar_t reserved3;
      uchar_t reserved4;
    } pgcode_4;
    struct pgcode_c
    {
      uchar_t 	data_length;	/* Sense data length */
      uchar_t	unused1[3];
      uchar_t	density;
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;

      uchar_t pg_code:6;	/* page code (should be c)	      */
      uchar_t :1;		/* reserved */
      uchar_t ps:1;		/* must be one			      */
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t :6;		/* reserved */
      uchar_t lpn:1;
      uchar_t nd:1;
      uchar_t reserved_0;
      uchar_t max_notches_1;	/* Maximum number of notches MSB */
      uchar_t max_notches_0;	/* Maximum number of notches LSB */
      uchar_t active_notch_1;	/* MSB */
      uchar_t active_notch_0;	/* LSB */
      uchar_t start_3;		/* MSB */
      uchar_t start_2;		
      uchar_t start_1;		
      uchar_t start_0;		/* LSB */
      uchar_t end_3;		/* MSB */
      uchar_t end_2;		
      uchar_t end_1;		
      uchar_t end_0;		/* LSB */
      uchar_t pages_notched[8];
    } pgcode_c;
  } params;
};

/**********************************************/
/* define the standard scsi window definition */
/**********************************************/
struct scsi_scan_win_def
{
  uchar_t   win_id;
  uchar_t   reserved_2;
  uchar_t   x_res[2];
  uchar_t   y_res[2];
  uchar_t   upper_left_x[4];
  uchar_t   upper_left_y[4];
  uchar_t   width[4];
  uchar_t   len[4];
  uchar_t   brightness;
  uchar_t   threshold;
  uchar_t   contrast;
  uchar_t   im_compo;
  uchar_t   pixel_bits;
  uchar_t   halftone_pat[2];
  uchar_t   pad_type:3;
  uchar_t   reserved_3:4;
  uchar_t   rif:1;
  uchar_t   byte_order[2];
  uchar_t   cmp_type;
  uchar_t   cmp_arg;
  uchar_t   reserved_4[6];
};


struct scsi_scan_datastatus
{
  uchar_t   win_id;
  uchar_t   unused;
  uchar_t   avail3;
  uchar_t   avail2;
  uchar_t   avail1;
  uchar_t   fill3;
  uchar_t   fill2;
  uchar_t   fill1;
};

#else

union scsi_cmd
{

	struct scsi_generic
	{
		uchar_t	opcode;
		uchar_t  :5;
		uchar_t  lun:3;
		uchar_t	bytes[10];
	} generic;

	struct scsi_test_unit_ready
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	unused[3];
		uchar_t	link:1;
		uchar_t	flag:4;
		uchar_t	:3;
	} test_unit_ready;

	struct scsi_send_diag
	{
		uchar_t	op_code;
		uchar_t	uol:1;
		uchar_t	dol:1;
		uchar_t	selftest:1;
		uchar_t	:1;
		uchar_t	pf:1;
		uchar_t	lun:3;
		uchar_t	unused[1];
		uchar_t	paramlen[2];
		uchar_t	link:1;
		uchar_t	flag:4;
		uchar_t	:3;
	} send_diag;

	struct scsi_receive_diag
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	unused[1];
		uchar_t	paramlen[2];
		uchar_t	link:1;
		uchar_t	flag:4;
		uchar_t	:3;
	} receive_diag;

	struct scsi_read_defect_data
	  {
	    uchar_t     op_code;
	    uchar_t	:5;
	    uchar_t	lun:3;	
	    uchar_t     format:3;
	    uchar_t     glist:1;
	    uchar_t     plist:1;
	    uchar_t     :3;
	    uchar_t     res[4];
	    uchar_t     len[2];	    
	    uchar_t	link:1;
	    uchar_t	flag:4;
	    uchar_t	:3;
	  } read_defect_data;

	struct scsi_sense
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;	
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} sense;

	struct scsi_inquiry
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;	
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} inquiry;

	struct scsi_mode_sense
	{
		uchar_t	op_code;
		uchar_t	:3;
		uchar_t	dbd:1;
		uchar_t	rsvd:1;
		uchar_t	lun:3;	
		uchar_t	page_code:6;
		uchar_t	page_ctrl:2;
		uchar_t	unused;
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} mode_sense;

	struct scsi_mode_select
	{
		uchar_t	op_code;
		uchar_t  sp:1;
		uchar_t	:3;
		uchar_t  pf:1;
		uchar_t	lun:3;	
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} mode_select;

	struct scsi_reassign_blocks
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;	
		uchar_t	unused[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} reassign_blocks;

	struct scsi_rw
	{
		uchar_t	op_code;
		uchar_t	addr_2:5;	/* Most significant */
		uchar_t	lun:3;
		uchar_t	addr_1;
		uchar_t	addr_0;		/* least significant */
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} rw;

	struct scsi_rw_big
	{
		uchar_t	op_code;
		uchar_t	rel_addr:1;
		uchar_t	:4;	/* Most significant */
		uchar_t	lun:3;
		uchar_t	addr_3;
		uchar_t	addr_2;
		uchar_t	addr_1;
		uchar_t	addr_0;		/* least significant */
		uchar_t	reserved;;
		uchar_t	length2;
		uchar_t	length1;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:4;
		uchar_t	vendor:2;
	} rw_big;

	struct scsi_rw_tape
	{
		uchar_t	op_code;
		uchar_t	fixed:1;
		uchar_t	:4;	
		uchar_t	lun:3;
		uchar_t	len[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} rw_tape;

	struct scsi_format
	  {
	        uchar_t op_code;
		uchar_t dlf:3;
		uchar_t cmplst:1;
		uchar_t fmtdat:1;
		uchar_t lun:3;
		uchar_t pattern;
		uchar_t interleave1;
		uchar_t interleave0;
		uchar_t link:1;
		uchar_t flag:1;
		uchar_t reserved:4;
		uchar_t vendor:2;
	  } format;


	struct scsi_read_capacity
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	addr_3;	/* Most Significant */
		uchar_t	addr_2;
		uchar_t	addr_1;
		uchar_t	addr_0;	/* Least Significant */
		uchar_t	unused[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;	
	} read_capacity;

	struct scsi_start_stop
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	unused[2];
		uchar_t	start:1;
		uchar_t	:7;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} start_stop;

	struct scsi_space
	{
		uchar_t	op_code;
		uchar_t	code:2;
		uchar_t	:3;
		uchar_t	lun:3;
		uchar_t	number[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} space;
#define SP_BLKS	0
#define SP_FILEMARKS 1
#define SP_SEQ_FILEMARKS 2
#define	SP_EOM	3

	struct scsi_write_filemarks
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	number[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} write_filemarks;

	struct scsi_rewind
	{
		uchar_t	op_code;
		uchar_t	immed:1;
		uchar_t	:4;
		uchar_t	lun:3;
		uchar_t	unused[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} rewind;

	struct scsi_load
	{
		uchar_t	op_code;
		uchar_t	immed:1;
		uchar_t	:4;
		uchar_t	lun:3;
		uchar_t	unused[2];
		uchar_t	load:1;
		uchar_t	reten:1;
		uchar_t	:6;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} load;
#define LD_UNLOAD 0
#define LD_LOAD 1

	struct scsi_reserve
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;	
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} reserve;

	struct scsi_release
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;	
		uchar_t	unused[2];
		uchar_t	length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} release;

	struct scsi_prevent
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	unused[2];
		uchar_t	prevent:1;
		uchar_t	:7;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} prevent;
#define	PR_PREVENT 1
#define PR_ALLOW   0

	struct scsi_blk_limits
	{
		uchar_t	op_code;
		uchar_t	:5;
		uchar_t	lun:3;
		uchar_t	unused[3];
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	} blk_limits;

	struct scsi_set_window
	  {
	        uchar_t  op_code;
		uchar_t  :5;
		uchar_t  lun:3;
		uchar_t  unused[4];
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	   } set_window;

	struct scsi_get_window
	  {
	        uchar_t  op_code;
		uchar_t  single:1;
		uchar_t  :4;
		uchar_t  lun:3;
		uchar_t  unused[3];
		uchar_t  win_id;
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t  link:1;
		uchar_t  flag:1;
		uchar_t  :6;
	  } get_window;

	struct scsi_scan
	  {
	        uchar_t  op_code;
		uchar_t  :5;
		uchar_t  lun:3;
		uchar_t  unused[2];
		uchar_t  length;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	  } scan;

	struct scsi_scan_read
	  {
	        uchar_t  op_code;
		uchar_t  :5;
		uchar_t  lun:3;
		uchar_t  datatype;
		uchar_t  unused;
		uchar_t  dataq2;
	        uchar_t  dataq1;
		uchar_t  length3;
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	  } scan_read;

	struct scsi_get_data_buffer_status
	  {
	        uchar_t  op_code;
		uchar_t  :5;
		uchar_t  lun:3;
		uchar_t  unused[5];
		uchar_t  length2;
		uchar_t  length1;
		uchar_t	link:1;
		uchar_t	flag:1;
		uchar_t	:6;
	  } scan_getbufstatus;
};

union mode_sel_data
{
	struct
	{
		uchar_t	unused[2];
		uchar_t	speed:4;
		uchar_t	buf_mode:3;
		uchar_t	:1;
		uchar_t	length;
		uchar_t	density;
		uchar_t	seg_len[3];
		uchar_t	unused2;
		uchar_t	blklen[3];
	}tape;
};

struct scsi_device_inquiry
{
	uchar_t	device_type:5;
	uchar_t	device_qualifier:3;
	uchar_t	dev_qual2:7;
	uchar_t	removable:1;
	uchar_t  ansi:3;
	uchar_t  ecma:3;
	uchar_t  iso:2;
	uchar_t	response_format:4;
	uchar_t  res1:2;
	uchar_t  trmiop:1;
	uchar_t  aenc:1;
	uchar_t	additional_length;
	uchar_t	unused[2];
	uchar_t  sftre:1;
	uchar_t  cmdq:1;
	uchar_t  res2:1;
	uchar_t	link:1;
	uchar_t  sync:1;
	uchar_t  wbus16:1;
	uchar_t  wbus32:1;
	uchar_t  reladr:1;
	char	vendor[8];
	char	product[16];
	char	revision[4];
	uchar_t	extra[8];
};


struct	scsi_sense_data
{
  union
    {
      struct
	{
	  uchar_t	error_code:4;
	  uchar_t	error_class:3;
	  uchar_t	valid:1;
	  uchar_t	blockhi:5;
	  uchar_t	vendor:3;
	  uchar_t	blockmed;
	  uchar_t	blocklow;
	} unextended;
      struct
	{
	  uchar_t	error_code:4;
	  uchar_t	error_class:3;
	  uchar_t	valid:1;
	  uchar_t	segment;
	  uchar_t	sense_key:4;
	  uchar_t	res:1;
	  uchar_t	ili:1;
	  uchar_t	eom:1;
	  uchar_t	filemark:1;
	  uchar_t	info[4];
	  uchar_t	extra_len;
	  /* allocate enough room to hold new stuff
	     uchar_t	cmd_spec_info[4];
	     uchar_t	add_sense_code;
	     uchar_t	add_sense_code_qual;
	     uchar_t	fru;
	     uchar_t	sense_key_spec_1:7;
	     uchar_t	sksv:1;
	     uchar_t	sense_key_spec_2;
	     uchar_t	sense_key_spec_3;
	     ( by increasing 16 to 26 below) */
	  uchar_t	extra_bytes[26];
	} extended;
    }ext;
};
struct	scsi_sense_data_new
{
  union
    {
      struct
	{
	  uchar_t	error_code:7;
	  uchar_t	valid:1;
	  uchar_t	blockhi:5;
	  uchar_t	vendor:3;
	  uchar_t	blockmed;
	  uchar_t	blocklow;
	} unextended;
      struct
	{
	  uchar_t	error_code:7;
	  uchar_t	valid:1;
	  uchar_t	segment;
	  uchar_t	sense_key:4;
	  uchar_t	:1;
	  uchar_t	ili:1;
	  uchar_t	eom:1;
	  uchar_t	filemark:1;
	  uchar_t	info[4];
	  uchar_t	extra_len;
	  uchar_t	cmd_spec_info[4];
	  uchar_t	add_sense_code;
	  uchar_t	add_sense_code_qual;
	  uchar_t	fru;
	  uchar_t	sense_key_spec_1:7;
	  uchar_t	sksv:1;
	  uchar_t	sense_key_spec_2;
	  uchar_t	sense_key_spec_3;
	  uchar_t	extra_bytes[16];
	} extended;
    }ext;
};


struct scsi_blk_limits_data
{
	uchar_t	reserved;
	uchar_t	max_length_2;	/* Most significant */
	uchar_t	max_length_1;
	uchar_t	max_length_0;	/* Least significant */
	uchar_t	min_length_1;	/* Most significant */
	uchar_t	min_length_0;	/* Least significant */
};

struct scsi_read_cap_data
{
	uchar_t	addr_3;	/* Most significant */
	uchar_t	addr_2;
	uchar_t	addr_1;
	uchar_t	addr_0;	/* Least significant */
	uchar_t	length_3;	/* Most significant */
	uchar_t	length_2;
	uchar_t	length_1;
	uchar_t	length_0;	/* Least significant */
};

struct scsi_reassign_blocks_data
{
	uchar_t	reserved[2];
	uchar_t	length_msb;
	uchar_t	length_lsb;
	uchar_t	dlbaddr_3;	/* defect logical block address (MSB) */
	uchar_t	dlbaddr_2;
	uchar_t	dlbaddr_1;
	uchar_t	dlbaddr_0;	/* defect logical block address (LSB) */
};

/* 
  The mode page codes for direct-access devices are shown in Table 8-47.

                         Table 8-47: Mode Page Codes 

==============================================================================
Page Code     Description                                           Section
------------------------------------------------------------------------------
   08h        Caching Page                                          8.3.3.1
   0Ah        Control Mode Page                                     7.3.3.1
   02h        Disconnect-Reconnect Page                             7.3.3.2
   05h        Flexible Disk Page                                    8.3.3.2
   03h        Format Device Page                                    8.3.3.3
   0Bh        Medium Types Supported Page                           8.3.3.4
   0Ch        Notch and Partition Page                              8.3.3.5
   09h        Peripheral Device Page                                7.3.3.3
   01h        Read-Write Error Recovery Page                        8.3.3.6
   04h        Rigid Disk Geometry Page                              8.3.3.7
   07h        Verify Error Recovery Page                            8.3.3.8
   00h        Vendor-Specific (does not require page format)
   06h        Reserved
0Dh - 1Fh     Reserved
   3Fh        Return all pages (valid only for the MODE SENSE command)
20h - 3Eh     Vendor-specific 


==============================================================================
                           Table 8-48: Caching Page

==============================================================================
  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
Byte |        |        |        |        |        |        |        |        |
==============================================================================
 0   |   PS   |Reserved|        Page Code (08h)                              |
-----|-----------------------------------------------------------------------|
 1   |                          Page Length (0Ah)                            |
-----|-----------------------------------------------------------------------|
 2   |                          Reserved          |  WCE   |   MF   |  RCD   |
-----|-----------------------------------------------------------------------|
 3   |   Demand Read Retention Priority  |     Write Retention Priority      |
-----|-----------------------------------------------------------------------|
 4   | (MSB)                                                                 |
-----|---                       Disable Pre-fetch Transfer Length         ---|
 5   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 6   | (MSB)                                                                 |
-----|---                       Minimum Pre-fetch                         ---|
 7   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 8   | (MSB)                                                                 |
-----|---                       Maximum Pre-fetch                         ---|
 9   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 10  | (MSB)                                                                 |
-----|---                       Maximum Pre-fetch Ceiling                 ---|
 11  |                                                                 (LSB) |
==============================================================================


                        Table 8-52: Format Device Page

==============================================================================
  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
Byte |        |        |        |        |        |        |        |        |
==============================================================================
 0   |   PS   |Reserved|            Page Code (03h)                          |
-----|-----------------------------------------------------------------------|
 1   |                              Page Length (16h)                        |
-----|-----------------------------------------------------------------------|
 2   | (MSB)                                                                 |
-----|---                  Tracks per Zone                                ---|
 3   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 4   | (MSB)                                                                 |
-----|---                  Alternate Sectors per Zone                     ---|
 5   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 6   | (MSB)                                                                 |
-----|---                  Alternate Tracks per Zone                      ---|
 7   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 8   | (MSB)                                                                 |
-----|---                  Alternate Tracks per Logical Unit              ---|
 9   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 10  | (MSB)                                                                 |
-----|---                  Sectors per Track                              ---|
 11  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 12  | (MSB)                                                                 |
-----|---                  Data Bytes per Physical Sector                 ---|
 13  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 14  | (MSB)                                                                 |
-----|---                  Interleave                                     ---|
 15  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 16  | (MSB)                                                                 |
-----|---                  Track Skew Factor                              ---|
 17  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 18  | (MSB)                                                                 |
-----|---                  Cylinder Skew Factor                           ---|
 19  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 20  |  SSEC  |  HSEC  |  RMB   |  SURF  |             Reserved              |
-----|-----------------------------------------------------------------------|
 21  |                                                                       |
- - -|- -                  Reserved                                       - -|
 23  |                                                                       |
==============================================================================

                            Table 8-54: Notch Page

==============================================================================
  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
Byte |        |        |        |        |        |        |        |        |
==============================================================================
 0   |   PS   |Reserved|         Page Code (0Ch)                             |
-----|-----------------------------------------------------------------------|
 1   |                           Page Length (16h)                           |
-----|-----------------------------------------------------------------------|
 2   |   ND   |  LPN   |         Reserved                                    |
-----|-----------------------------------------------------------------------|
 3   |                           Reserved                                    |
-----|-----------------------------------------------------------------------|
 4   | (MSB)                                                                 |
-----|---                        Maximum Number of Notches                ---|
 5   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 6   | (MSB)                                                                 |
-----|---                        Active Notch                             ---|
 7   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 8   | (MSB)                                                                 |
- - -|- -                        Starting Boundary                        - -|
 11  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 12  | (MSB)                                                                 |
- - -|- -                        Ending Boundary                          - -|
 15  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 16  | (MSB)                                                                 |
- - -|- -                        Pages Notched                            - -|
 23  |                                                                 (LSB) |
==============================================================================

                  Table 8-61: Rigid Disk Drive Geometry Page

==============================================================================
  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
Byte |        |        |        |        |        |        |        |        |
==============================================================================
 0   |   PS   |Reserved|         Page Code (04h)                             |
-----|-----------------------------------------------------------------------|
 1   |                           Page Length (16h)                           |
-----|-----------------------------------------------------------------------|
 2   | (MSB)                                                                 |
- - -|- -                        Number of Cylinders                      - -|
 4   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 5   |                           Number of Heads                             |
-----|-----------------------------------------------------------------------|
 6   | (MSB)                                                                 |
- - -|- -               Starting Cylinder-Write Precompensation           - -|
 8   |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 9   | (MSB)                                                                 |
- - -|- -               Starting Cylinder-Reduced Write Current           - -|
 11  |                                                                 (LSB) |
-----|-+---------------------------------------------------------------------|
 12  | (MSB)                                                                 |
-----|---                        Drive Step Rate                          ---|
 13  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 14  | (MSB)                                                                 |
- - -|- -                        Landing Zone Cylinder                    - -|
 16  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 17  |                           Reserved                  |       RPL       |
-----|-----------------------------------------------------------------------|
 18  |                           Rotational Offset                           |
-----|-----------------------------------------------------------------------|
 19  |                           Reserved                                    |
-----|-----------------------------------------------------------------------|
 20  | (MSB)                                                                 |
-----|---                        Medium Rotation Rate                     ---|
 21  |                                                                 (LSB) |
-----|-----------------------------------------------------------------------|
 22  |                           Reserved                                    |
-----|-----------------------------------------------------------------------|
 23  |                           Reserved                                    |
==============================================================================

*/

struct scsi_mode_sense_data
{
  union
  {
    struct pgcode_3		/* FORMAT DEVICE PAGE */
    {
      uchar_t	data_length;	/* Sense data length */
      uchar_t	unused1[3];
      uchar_t	density;
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;
      uchar_t pg_code:6;	/* page code (should be 3)	      */
      uchar_t :2;		
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t trk_z_1;	/* tracks per zone (MSB)	      */
      uchar_t trk_z_0;	/* tracks per zone (LSB)	      */
      uchar_t alt_sec_1;	/* alternate sectors per zone (MSB)   */
      uchar_t alt_sec_0;	/* alternate sectors per zone (LSB)   */
      uchar_t alt_trk_z_1;	/* alternate tracks per zone (MSB)    */
      uchar_t alt_trk_z_0;	/* alternate tracks per zone (LSB)    */
      uchar_t alt_trk_v_1;	/* alternate tracks per volume (MSB)  */
      uchar_t alt_trk_v_0;	/* alternate tracks per volume (LSB)  */
      uchar_t ph_sec_t_1;	/* physical sectors per track (MSB)   */
      uchar_t ph_sec_t_0;	/* physical sectors per track (LSB)   */
      uchar_t bytes_s_1;	/* bytes per sector (MSB)	      */
      uchar_t bytes_s_0;	/* bytes per sector (LSB)	      */
      uchar_t interleave_1;/* interleave (MSB)		      */
      uchar_t interleave_0;/* interleave (LSB)		      */
      uchar_t trk_skew_1;	/* track skew factor (MSB)	      */
      uchar_t trk_skew_0;	/* track skew factor (LSB)	      */
      uchar_t cyl_skew_1;	/* cylinder skew (MSB)		      */
      uchar_t cyl_skew_0;	/* cylinder skew (LSB)		      */
      uchar_t reserved1:4;
      uchar_t surf:1;
      uchar_t rmb:1;
      uchar_t hsec:1;
      uchar_t ssec:1;
      uchar_t reserved2;
      uchar_t reserved3;
      uchar_t reserved4;
    } pgcode_3;
    struct pgcode_4		/* RIGID DISK GEOMETRY PAGE */
    {
      uchar_t 	data_length;	/* Sense data length */
      uchar_t	unused1[3];
      uchar_t	density;
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;
      uchar_t pg_code:7;	/* page code (should be 4)	      */
      uchar_t mbone:1;	/* must be one			      */
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t ncyl_2;	/* number of cylinders (MSB)	      */
      uchar_t ncyl_1;	/* number of cylinders 		      */
      uchar_t ncyl_0;	/* number of cylinders (LSB)	      */
      uchar_t nheads;	/* number of heads 		      */
      uchar_t st_cyl_wp_2; /* starting cyl., write precomp (MSB) */
      uchar_t st_cyl_wp_1; /* starting cyl., write precomp       */
      uchar_t st_cyl_wp_0; /* starting cyl., write precomp (LSB) */
      uchar_t st_cyl_rwc_2;/* starting cyl., red. write cur (MSB)*/
      uchar_t st_cyl_rwc_1;/* starting cyl., red. write cur      */
      uchar_t st_cyl_rwc_0;/* starting cyl., red. write cur (LSB)*/
      uchar_t driv_step_1; /* drive step rate (MSB)	      */
      uchar_t driv_step_0; /* drive step rate (LSB)	      */
      uchar_t land_zone_2; /* landing zone cylinder (MSB)	      */
      uchar_t land_zone_1; /* landing zone cylinder 	      */
      uchar_t land_zone_0; /* landing zone cylinder (LSB)	      */
      uchar_t rpl:2;
      uchar_t reserved1:6;
      uchar_t rot_offset;
      uchar_t reserved2;
      uchar_t rot_rate_1;
      uchar_t rot_rate_0;
      uchar_t reserved3;
      uchar_t reserved4;
    } pgcode_4;
    struct pgcode_c
    {
      uchar_t 	data_length;	/* Mode parameter block header */
      uchar_t	unused1[3];

      uchar_t	density;	/* Mode parameter block descriptor */
      uchar_t	nblocks_2;
      uchar_t	nblocks_1;
      uchar_t	nblocks_0;
      uchar_t	unused2;
      uchar_t	blksz_2;
      uchar_t	blksz_1;
      uchar_t	blksz_0;

      uchar_t pg_code:6;	/* page code (should be c)	      */
      uchar_t :1;		/* reserved */
      uchar_t ps:1;		/* must be one			      */
      uchar_t pg_length;	/* page length (should be 0x16)	      */
      uchar_t :6;		/* reserved */
      uchar_t lpn:1;
      uchar_t nd:1;
      uchar_t reserved_0;
      uchar_t max_notches_1;	/* Maximum number of notches MSB */
      uchar_t max_notches_0;	/* Maximum number of notches LSB */
      uchar_t active_notch_1;	/* MSB */
      uchar_t active_notch_0;	/* LSB */
      uchar_t start_3;		/* MSB */
      uchar_t start_2;		
      uchar_t start_1;		
      uchar_t start_0;		/* LSB */
      uchar_t end_3;		/* MSB */
      uchar_t end_2;		
      uchar_t end_1;		
      uchar_t end_0;		/* LSB */
      uchar_t pages_notched[8];
    } pgcode_c;
  } params;
} ;

/**********************************************/
/* define the standard scsi window definition */
/**********************************************/
struct scsi_scan_win_def
{
  uchar_t   win_id;
  uchar_t   reserved_2;
  uchar_t   x_res[2];
  uchar_t   y_res[2];
  uchar_t   upper_left_x[4];
  uchar_t   upper_left_y[4];
  uchar_t   width[4];
  uchar_t   len[4];
  uchar_t   brightness;
  uchar_t   threshold;
  uchar_t   contrast;
  uchar_t   im_compo;
  uchar_t   pixel_bits;
  uchar_t   halftone_pat[2];
  uchar_t   pad_type:3;
  uchar_t   reserved_3:4;
  uchar_t   rif:1;
  uchar_t   byte_order[2];
  uchar_t   cmp_type;
  uchar_t   cmp_arg;
  uchar_t   reserved_4[6];
};

struct scsi_scan_datastatus
{
  uchar_t   win_id;
  uchar_t   unused;
  uchar_t   avail3;
  uchar_t   avail2;
  uchar_t   avail1;
  uchar_t   fill3;
  uchar_t   fill2;
  uchar_t   fill1;
};

#endif /* BIG_ENDIAN */

/* Command opcode */

/*
 * Opcodes
 */

#define	TEST_UNIT_READY         0x00
#define REWIND                  0x01
#define REQUEST_SENSE           0x03
#define FORMAT                  0x04
#define	READ_BLK_LIMITS	        0x05
#define	REASSIGN_BLOCKS	        0x07
#define	READ_COMMAND            0x08
#define WRITE_COMMAND           0x0a
#define	WRITE_FILEMARKS	        0x10
#define	SPACE			0x11
#define INQUIRY			0x12
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1a
#define START_STOP		0x1b
#define LOAD_UNLOAD		0x1b /* same as above */
#define RESERVE      	        0x16
#define RELEASE_DEV             0x17
#define RECEIVE_DIAGNOSTIC      0x1c
#define SEND_DIAGNOSTIC         0x1d
#define PREVENT_ALLOW	        0x1e
#define	READ_CAPACITY	        0x25
#define	READ_BIG		0x28
#define WRITE_BIG		0x2a
#define MOVE_MEDIUM             0xa5
#define POSITION_TO_ELEMENT     0x2b
#define READ_ELEMENT_STATUS     0xb8
#define SET_WINDOW              0x24
#define GET_WINDOW              0x25
#define SCAN                    0x1B
#define GET_BUFSTATUS           0x34
#define READ_DEFECT_LIST        0x37





/*
**	Messages
*/

#define	M_COMPLETE	(0x00)
#define	M_EXTENDED	(0x01)
#define	M_SAVE_DP	(0x02)
#define	M_RESTORE_DP	(0x03)
#define	M_DISCONNECT	(0x04)
#define	M_ID_ERROR	(0x05)
#define	M_ABORT		(0x06)
#define	M_REJECT	(0x07)
#define	M_NOOP		(0x08)
#define	M_PARITY	(0x09)
#define	M_LCOMPLETE	(0x0a)
#define	M_FCOMPLETE	(0x0b)
#define	M_RESET		(0x0c)
#define	M_ABORT_TAG	(0x0d)
#define	M_CLEAR_QUEUE	(0x0e)
#define	M_INIT_REC	(0x0f)
#define	M_REL_REC	(0x10)
#define	M_TERMINATE	(0x11)
#define	M_SIMPLE_TAG	(0x20)
#define	M_HEAD_TAG	(0x21)
#define	M_ORDERED_TAG	(0x22)
#define	M_IGN_RESIDUE	(0x23)
#define	M_IDENTIFY   	(0x80)

#define	M_X_MODIFY_DP	(0x00)
#define	M_X_SYNC_REQ	(0x01)
#define	M_X_WIDE_REQ	(0x03)

/*
**	Status
*/

#define	S_GOOD		(0x00)
#define	S_CHECK_COND	(0x02)
#define	S_COND_MET	(0x04)
#define	S_BUSY		(0x08)
#define	S_INT		(0x10)
#define	S_INT_COND_MET	(0x14)
#define	S_CONFLICT	(0x18)
#define	S_TERMINATED	(0x20)
#define	S_QUEUE_FULL	(0x28)
#define	S_ILLEGAL	(0xff)
#define	S_SENSE		(0x80)


/*
 *  SENSE KEYS
 */

#define NO_SENSE            0x00
#define RECOVERED_ERROR     0x01
#define NOT_READY           0x02
#define MEDIUM_ERROR        0x03
#define HARDWARE_ERROR      0x04
#define ILLEGAL_REQUEST     0x05
#define UNIT_ATTENTION      0x06
#define DATA_PROTECT        0x07
#define BLANK_CHECK         0x08
#define COPY_ABORTED        0x0a
#define ABORTED_COMMAND     0x0b
#define VOLUME_OVERFLOW     0x0d
#define MISCOMPARE          0x0e


#if 0
/*
** Status Byte defns; we have some synonyms here for other drivers.
*/
#define	SCSI_OK		        0x00
#define	SCSI_CHECK		0x02
#define SCSI_MET                0x04
#define	SCSI_BUSY		0x08	
#define SCSI_INTERM		0x10
#define SCSI_INTERM_MET         0x14
#define SCSI_RESCONFLICT        0x18
#define SCSI_CMD_TERM           0x22
#define SCSI_Q_FULL             0x28
#endif

/*
 * sense data format
 */
#define T_DIRECT	0
#define T_SEQUENTIAL	1
#define T_PRINTER	2
#define T_PROCESSOR	3
#define T_WORM		4
#define T_READONLY	5
#define T_SCANNER 	6
#define T_OPTICAL 	7
#define T_CHANGER	8
#define T_COMM		9

#define T_REMOV		1
#define	T_FIXED		0

#if 0
/* SCSI Messages */

/* one byte messages */
#define M_ABORT                  0x06
#define M_BUSDEV_RESET           0x0C
#define M_CCOMPLETED             0x00
#define M_DISCONNECT             0x04
#define M_IDENTIFY_WO_DIS        0x80
#define M_IDENTIFY_W_DIS         0xC0
#define M_INIT_DETECT_ERROR      0x05
#define M_MESG_PARITY_ERROR      0x09
#define M_MESG_REJECT            0x07
#define M_NOOP                   0x08
#define M_RELEASE_RECOVERY       0x10
#define M_RESTORE_PTRS           0x03
#define M_SAVE_DATA_PTRS         0x02
#define M_TERMINATE_IO           0x11

/* extended messages */
#define M_EXTENDED_MESG          0x01
#define M_SYNC_DXFER_REQ         0x01
#define M_MODIFY_DATA_PTR        0x00
#endif



#endif /* __SCSI_H__ */
