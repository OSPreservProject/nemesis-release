/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      IREF.def.edit (IREF.def.c in build tree)
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Definition of types in the 'mythical' interface IREF which
**	defines all predefined Middl types and all interfaces in Nemesis
**	(including IREF).
** 
** ENVIRONMENT: 
**
**      Compiled into the TypeSystem module
** 
** ID : $Id: IREF.def.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis_types.h>
#include <TypeSystem_st.h>


#define PREDEFINED_TYPE_REP(n1,n2,n3) \
static TypeRep_t type_##n1##__rep = { \
  {  TypeSystem_Predefined__code, TypeSystem_Predefined_##n3 }, \
  {  Type_Code__code, n1##__code }, \
  n2, \
  &IREF__intf, \
  sizeof (n1) \
}


PREDEFINED_TYPE_REP(uint8_t,"OCTET", Octet);
PREDEFINED_TYPE_REP(uint16_t,"SHORT CARDINAL", ShortCardinal);
PREDEFINED_TYPE_REP(uint32_t,"CARDINAL", Cardinal);
PREDEFINED_TYPE_REP(uint64_t,"LONG CARDINAL", LongCardinal);
PREDEFINED_TYPE_REP(int8_t,"CHAR", Char);
PREDEFINED_TYPE_REP(int16_t,"SHORT INTEGER", ShortInteger);
PREDEFINED_TYPE_REP(int32_t,"INTEGER", Integer);
PREDEFINED_TYPE_REP(int64_t,"LONG INTEGER", LongInteger);
PREDEFINED_TYPE_REP(float32_t,"REAL", Real);
PREDEFINED_TYPE_REP(float64_t,"LONG REAL", LongReal);
PREDEFINED_TYPE_REP(bool_t,"BOOLEAN", Boolean);
PREDEFINED_TYPE_REP(string_t,"STRING", String);
PREDEFINED_TYPE_REP(word_t,"DANGEROUS WORD", Word);
PREDEFINED_TYPE_REP(addr_t,"DANGEROUS ADDRESS", Address);

static TypeRep_t * const IREF__types[] = {
  &type_uint8_t__rep,
  &type_uint16_t__rep,
  &type_uint32_t__rep,
  &type_uint64_t__rep,
  &type_int8_t__rep,
  &type_int16_t__rep,
  &type_int32_t__rep,
  &type_int64_t__rep,
  &type_float32_t__rep,
  &type_float64_t__rep,
  &type_bool_t__rep,
  &type_string_t__rep,
  &type_word_t__rep,
  &type_addr_t__rep,
  (TypeRep_t *)0
};

static Type_Code const IREF__needs[] = {
  TCODE_NONE
};

extern Interface_cl meta_cl;
Intf_st IREF__intf ={
    { 
	{ TypeSystem_Iref__code, (word_t)&meta_cl },    /* any & cl.ptr */
	{ Type_Code__code, IREF_clp__code }, /* Type Code    */ 
	"IREF",                              /* Textual name */
	&IREF__intf,                         /* Scope	     */ 
	sizeof(Interface_clp)                /* Size         */
    },
    /* Needs list   */ IREF__needs,
    /* No. of needs */ 0,
    /* Types list   */ IREF__types,
    /* No. of types */ 15,
    /* Local flag   */ True,
    /* Supertype    */ Interface_clp__code,
    NULL,
    0,
    NULL,
    0
};

