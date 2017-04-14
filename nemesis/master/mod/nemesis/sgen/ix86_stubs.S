/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/sgen/ix86_stubs.S
**
** FUNCTIONAL DESCRIPTION:
** 
**	Provides assembler stubs to swizzle the first parameter (closure
**  	pointer) of an invocation into the generic_op_st for that
**	operation
** 
** 
**
*/


        .text

	.globl  Generic_op

  
/* esp + 4 contains the closure. The state of this closure is an
	IDCClientStubs_State record. Offset 24 with this is the stubs
	state, consisting of an array of generic_op_st records, each
	of which are 12 bytes long. So for operation n, offset 12*n
	from the stubs state pointer is the address of the required
	record, which should be placed in esp + 4. Offset 8 in this
	record contains the function to jump through */
      
#define OP(n)   \
	.align 4		;  \
op_##n:	                           \
	movl     4(%esp),   %eax;  \
	movl     4(%eax),   %eax;  \
	movl    24(%eax),   %eax;  \
	add       $(12*n),   %eax; \
	movl       %eax,  4(%esp); \
	jmp      *8(%eax)
  

  OP(0)
  OP(1)
  OP(2)
  OP(3)
  OP(4)
  OP(5)
  OP(6)
  OP(7)
  OP(8)
  OP(9)
  OP(10)
  OP(11)
  OP(12)
  OP(13)
  OP(14)
  OP(15)
  OP(16)
  OP(17)
  OP(18)
  OP(19)
  OP(20)
  OP(21)
  OP(22)
  OP(23)
  OP(24)
  OP(25)
  OP(26)
  OP(27)
  OP(28)
  OP(29)
  OP(30)
  OP(31)
  OP(32)
  OP(33)
  OP(34)
  OP(35)
  OP(36)
  OP(37)
  OP(38)
  OP(39)

  .align  16
Generic_op:
  .long   op_0
  .long	  op_1
  .long   op_2
  .long   op_3
  .long   op_4
  .long   op_5
  .long   op_6
  .long	  op_7
  .long   op_8
  .long   op_9
  .long   op_10
  .long   op_11
  .long   op_12
  .long	  op_13
  .long   op_14
  .long   op_15
  .long   op_16
  .long   op_17
  .long   op_18
  .long   op_19
  .long   op_20
  .long	  op_21
  .long   op_22
  .long   op_23
  .long   op_24
  .long   op_25
  .long   op_26
  .long	  op_27
  .long   op_28
  .long   op_29
  .long   op_30
  .long   op_31
  .long   op_32
  .long	  op_33
  .long   op_34
  .long   op_35
  .long   op_36
  .long   op_37
  .long   op_38
  .long   op_39

