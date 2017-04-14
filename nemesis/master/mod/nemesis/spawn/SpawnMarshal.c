/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	ShmMarshal.c
**
** FUNCTIONAL DESCRIPTION:
**
**	Single client, single server implementation of IDC.if over
**	shared memory and a pair of event channels.
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: SpawnMarshal.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <string.h>

#include <IDC.ih>

#define TRC(_x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}

/* 
 * Macros
 */

#define ROUNDUP(p,TYPE)	( ((p) + sizeof (TYPE) - 1) & -sizeof (TYPE) )

/*
 * Prototypes
 */

static	IDC_mString_fn		mString_m;
static	IDC_uString_fn		uString_m;

static	IDC_mByteArray_fn	mByteArray_m;
static	IDC_uByteArray_fn	uByteArray_m;

static	IDC_mByteSequence_fn	mByteSequence_m;
static	IDC_uByteSequence_fn	uByteSequence_m;


#if 0

 /* This lot are implemented with macros in IDCMacros.h */

 static	IDC_mShortCardinal_fn	mShortCardinal_m;
 static	IDC_mCardinal_fn	mCardinal_m;
 static	IDC_mLongCardinal_fn	mLongCardinal_m;
 static	IDC_mShortInteger_fn	mShortInteger_m;
 static	IDC_mInteger_fn		mInteger_m;
 static	IDC_mLongInteger_fn	mLongInteger_m;
 static	IDC_mReal_fn		mReal_m;
 static	IDC_mLongReal_fn	mLongReal_m;
 static	IDC_mOctet_fn		mOctet_m;
 static	IDC_mChar_fn		mChar_m;
 static	IDC_mAddress_fn		mAddress_m;
 static	IDC_mWord_fn		mWord_m;
 static	IDC_mNetworkCardinal_fn	mNetworkCardinal_m;
 static	IDC_uShortCardinal_fn	uShortCardinal_m;
 static	IDC_uCardinal_fn	uCardinal_m;
 static	IDC_uLongCardinal_fn	uLongCardinal_m;
 static	IDC_uShortInteger_fn	uShortInteger_m;
 static	IDC_uInteger_fn		uInteger_m;
 static	IDC_uLongInteger_fn	uLongInteger_m;
 static	IDC_uReal_fn		uReal_m;
 static	IDC_uLongReal_fn	uLongReal_m;
 static	IDC_uOctet_fn		uOctet_m;
 static	IDC_uChar_fn		uChar_m;
 static	IDC_uAddress_fn		uAddress_m;
 static	IDC_uWord_fn		uWord_m;
 static	IDC_uNetworkCardinal_fn	uNetworkCardinal_m;
#endif

static  IDC_op ms = {

  0,			/* mBoolean_m,		*/
  0,			/* mShortCardinal_m,	*/
  0,			/* mCardinal_m,		*/
  0,			/* mLongCardinal_m,	*/
  0,			/* mShortInteger_m,	*/
  0,			/* mInteger_m,		*/
  0,			/* mLongInteger_m,	*/
  0,			/* mReal_m,		*/
  0,			/* mLongReal_m,		*/
  0,			/* mOctet_m,		*/
  0,			/* mChar_m,		*/
  0,			/* mAddress_m,		*/
  0,			/* mWord_m,		*/

  0,			/* uBoolean_m,		*/
  0,			/* uShortCardinal_m,	*/
  0,			/* uCardinal_m,		*/
  0,			/* uLongCardinal_m,	*/
  0,			/* uShortInteger_m,	*/
  0,			/* uInteger_m,		*/
  0,			/* uLongInteger_m,	*/
  0,			/* uReal_m,		*/
  0,			/* uLongReal_m,		*/
  0,			/* uOctet_m,		*/
  0,			/* uChar_m,		*/
  0,			/* uAddress_m,		*/
  0,			/* uWord_m,		*/

  mString_m,
  uString_m,

  mByteSequence_m,
  uByteSequence_m,
  
  0,                    /* mStartSequence_m     */
  0,                    /* uStartSequence_m     */

  0,                    /* mSequenceElement_m   */
  0,                    /* uSequenceElement_m   */

  0,                    /* mEndSequence_m       */
  0,                    /* uEndSequence_m       */

  0,                    /* mCustom_m            */
  0,                    /* uCustom_m            */

  mByteArray_m,
  uByteArray_m,

  0,			/* mBitSet_m,		*/
  0			/* uBitSet_m,		*/

};

/* 
 * Export it
 */

static const IDC_cl ShmIDC_cl = { &ms, NULL };

CL_EXPORT (IDC, SpawnIDC, ShmIDC_cl);


/*-------------------------------------------------------- Entry Points ---*/

/* 
 * Marshalling Strings
 */

static bool_t
mString_m (IDC_cl *self, IDC_BufferDesc bd, string_t s)
{
  word_t	len;

  len = (s) ? strlen (s) + 1 : 0;

  if (bd->space < len + sizeof (word_t))
    return False;

  *(((word_t *)(bd->ptr))++) = len;

  if (len)
  {
    strcpy (bd->ptr, s);
    ((char *)(bd->ptr)) += ROUNDUP (len, word_t);
  }

  return True;
}

static bool_t
uString_m (IDC_cl *self, IDC_BufferDesc bd, string_t *s)
{
  word_t	len;

  if (bd->space < sizeof (word_t))
    return False;

  len = *((word_t *)(bd->ptr))++;

  if (!len)
  {
    *s = NULL;
    return True;
  }
    
  if (!(*s = Heap$Malloc (bd->heap, len)))
    return False;
    
  memcpy (*s, bd->ptr, len);

  ((char *)(bd->ptr)) += ROUNDUP (len, word_t);

  return True;
}


/* 
 * Marshalling ByteArrays
 */

static bool_t
mByteArray_m (IDC_cl *self, IDC_BufferDesc bd, uint8_t *ba, word_t len)
{
  word_t	aligned = ROUNDUP (len, word_t);

  if (bd->space < aligned)
    return False;

  memcpy (bd->ptr, ba, len);
  ((char *)(bd->ptr)) += aligned;
  return True;
}

static bool_t
uByteArray_m (IDC_cl *self, IDC_BufferDesc bd, uint8_t *ba, word_t len)
{
  word_t	aligned = ROUNDUP (len, word_t);

  if (bd->space < aligned)
    return False;

  memcpy (ba, bd->ptr, len);
  ((char *)(bd->ptr)) += aligned;
  return True;
}


/* 
 * Marshalling ByteSequences
 */

static bool_t
mByteSequence_m (IDC_cl *self, IDC_BufferDesc bd, const IDC_ByteSequence *bs) {

    int len = bs->len;

    if (bd->space < len + sizeof (word_t))
	return False;
    
    *(((word_t *)(bd->ptr))++) = len;
    
    if (len)
    {
	memcpy (bd->ptr, bs->data, len);
	((char *)(bd->ptr)) += ROUNDUP (len, word_t);
    }
    
    return True;
}

static bool_t
uByteSequence_m (IDC_cl *self, 
		 IDC_BufferDesc bd, 
		 IDC_ByteSequence *bs) {
    word_t	len;
    
    if (bd->space < sizeof (word_t))
	return False;
    
    len  = *((word_t *)(bd->ptr))++;
    
    if (!len)
    {
	bs->len = bs->blen = 0;
	bs->data = bs->base = NULL;
	return True;
    }
    
    bs->len = bs->blen = len;
    
    if (!(bs->data = bs->base = Heap$Malloc (bd->heap, len)))
	return False;
    
    memcpy (bs->data, bd->ptr, len);
    
    ((char *)(bd->ptr)) += ROUNDUP (len, word_t);
    
    return True;
}

/*
 * End ShmMarshal.c
 */


