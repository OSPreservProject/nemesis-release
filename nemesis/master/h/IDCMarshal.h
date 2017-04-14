/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      IDCMacros.h
** 
** DESCRIPTION:
** 
**      Marshalling macros for IDC.
** 
** ID : $Id: IDCMarshal.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/
#ifndef __IDCMacros_h__
#define __IDCMacros_h__

#include <IDC.ih>
#include <Heap.ih>

#undef  IDC_mWord
#undef  IDC_uWord

#define IDC_m64bit(self,_bd, x) \
(((_bd)->space >= sizeof(uint64_t)) && \
 ((*(((uint64_t *)(_bd)->ptr)++)=(x)), (_bd)->space -= 8, True))

#define IDC_u64bit(self,_bd, x) \
(((_bd)->space >= sizeof(uint64_t)) && \
 (*(x)=(*((uint64_t *)(_bd)->ptr)++), (_bd)->space -= 8, True))

#define IDC_mWord(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 ((*(((word_t *)(_bd)->ptr)++)=(x)), (_bd)->space -= sizeof(word_t), True))

#define IDC_uWord(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 (*(x)=(*((word_t *)(_bd)->ptr)++), (_bd)->space -= sizeof(word_t), True))

#define IDC_m32bit(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 ((*((uint32_t *)(((word_t *)(_bd)->ptr)++)))=(x), (_bd)->space -= sizeof(word_t), True))

#define IDC_u32bit(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 (*(x)=(*(uint32_t *)(((word_t *)(_bd)->ptr)++)), (_bd)->space -= sizeof(word_t), True))

#define IDC_u16bit(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 (*(x)=(uint16_t)(*(uint32_t *)(((word_t *)(_bd)->ptr)++)), (_bd)->space -= sizeof(word_t), True))

#define IDC_m8bit(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 ((*((uint8_t *)(((word_t *)(_bd)->ptr)++)))=(x), (_bd)->space -= sizeof(word_t), True))

#define IDC_u8bit(self,_bd, x) \
(((_bd)->space >= sizeof(word_t)) && \
 (*(x)=(*(uint8_t *)(((word_t *)(_bd)->ptr)++)), (_bd)->space -= sizeof(word_t), True))


#undef  IDC_mBoolean
#undef  IDC_mShortCardinal
#undef  IDC_mCardinal
#undef  IDC_mLongCardinal
#undef  IDC_mShortInteger
#undef  IDC_mInteger
#undef  IDC_mLongInteger
#undef  IDC_mReal
#undef  IDC_mLongReal
#undef  IDC_mSet
#undef  IDC_mAddress
#undef  IDC_mOctet
#undef  IDC_mChar
#undef  IDC_mNetworkCardinal

#define IDC_mBoolean(self,_bd, x)	IDC_m32bit(self,_bd, (uint32_t)(x))
#define IDC_mShortCardinal(self,_bd, x)	IDC_m32bit(self,_bd, (uint32_t)(x))
#define IDC_mCardinal(self,_bd, x)	IDC_m32bit(self,_bd, (uint32_t)(x))
#define IDC_mLongCardinal(self,_bd, x)	IDC_m64bit(self,_bd, (uint64_t)(x))
#define IDC_mShortInteger(self,_bd, x)	IDC_m32bit(self,_bd, (uint32_t)(x))
#define IDC_mInteger(self,_bd, x)	IDC_m32bit(self,_bd, (uint32_t)(x))
#define IDC_mLongInteger(self,_bd, x)	IDC_m64bit(self,_bd, (uint64_t)(x))
#define IDC_mReal(self,_bd, x)		barf
#define IDC_mLongReal(self,_bd, x)	barf
#define IDC_mSet(self,_bd, x)		IDC_m64bit(self,_bd, (uint64_t)(x))
#define IDC_mAddress(self,_bd, x)	IDC_mWord (self,_bd, (word_t  )(x))
#define IDC_mOctet(self,_bd,x)		IDC_m8bit (self,_bd, (uint8_t )(x))
#define IDC_mChar(self,_bd,x)		IDC_m8bit (self,_bd, (uint8_t )(x))
#define IDC_mNetworkCardinal(self,_bd, x) IDC_m32bit(self,_bd, (uint32_t)(x))

#undef  IDC_uBoolean
#undef  IDC_uShortCardinal
#undef  IDC_uCardinal
#undef  IDC_uLongCardinal
#undef  IDC_uShortInteger
#undef  IDC_uInteger
#undef  IDC_uLongInteger
#undef  IDC_uReal
#undef  IDC_uLongReal
#undef  IDC_uSet
#undef  IDC_uAddress
#undef  IDC_uOctet
#undef  IDC_uChar
#undef  IDC_uNetworkCardinal

#define IDC_uBoolean(self,_bd, x)	IDC_u32bit(self,_bd, (uint32_t *)(x))
#define IDC_uShortCardinal(self,_bd, x)	IDC_u16bit(self,_bd, (uint16_t *)(x))
#define IDC_uCardinal(self,_bd, x)	IDC_u32bit(self,_bd, (uint32_t *)(x))
#define IDC_uLongCardinal(self,_bd, x)	IDC_u64bit(self,_bd, (uint64_t *)(x))
#define IDC_uShortInteger(self,_bd, x)	IDC_u16bit(self,_bd, (uint16_t *)(x))
#define IDC_uInteger(self,_bd, x)	IDC_u32bit(self,_bd, (uint32_t *)(x))
#define IDC_uLongInteger(self,_bd, x)	IDC_u64bit(self,_bd, (uint64_t *)(x))
#define IDC_uReal(self,_bd, x)		barf
#define IDC_uLongReal(self,_bd, x)	barf
#define IDC_uSet(self,_bd, x)		IDC_u64bit(self,_bd, (uint64_t *)(x))
#define IDC_uAddress(self,_bd, x)	IDC_uWord (self,_bd, (word_t   *)(x))
#define IDC_uOctet(self,_bd, x)		IDC_u8bit (self,_bd, (uint8_t  *)(x))
#define IDC_uChar(self,_bd, x)		IDC_u8bit (self,_bd, (uint8_t  *)(x))
#define IDC_uNetworkCardinal(self,_bd, x) IDC_u32bit(self,_bd, (uint32_t *)(x))

#define IDC_fBoolean(self,_bd, x)	/* Null Op */
#define IDC_fShortCardinal(self,_bd, x)	/* Null Op */
#define IDC_fCardinal(self,_bd, x)	/* Null Op */
#define IDC_fLongCardinal(self,_bd, x)	/* Null Op */
#define IDC_fShortInteger(self,_bd, x)	/* Null Op */
#define IDC_fInteger(self,_bd, x)	/* Null Op */
#define IDC_fLongInteger(self,_bd, x)	/* Null Op */
#define IDC_fReal(self,_bd, x)		/* Null Op */
#define IDC_fLongReal(self,_bd, x)	/* Null Op */
#define IDC_fSet(self,_bd, x)		/* Null Op */
#define IDC_fAddress(self,_bd, x)	/* Null Op */
#define IDC_fWord(self,_bd, x)		/* Null Op */
#define IDC_fOctet(self,_bd, x)		/* Null Op */
#define IDC_fChar(self,_bd, x)		/* Null Op */
#define IDC_fNetworkCardinal(self,_bd, x)	/* Null Op */

#undef  IDC_fString
#define IDC_fString(self,_bd, str) \
  do { if (str) FREE (str); } while(0)
    
#define IDC_fByteArray(self,_bd, ptr, len)	/* Null Op */

#undef  IDC_fByteSequence
#define IDC_fByteSequence(self,_bd, ptr, len) \
  do { if (ptr) FREE (ptr); } while(0)


#define IDC_M_bytes		    IDC$mByteArray
#define IDC_U_bytes		    IDC$uByteArray
#define IDC_F_bytes		    IDC_fByteArray

#define IDC_M_counted_bytes	    IDC$mByteSequence
#define IDC_U_counted_bytes	    IDC$uByteSequence
#define IDC_F_counted_bytes	    IDC_fByteSequence

#define IDC_M_bool_t                IDC_mBoolean        
#define IDC_M_uint16_t              IDC_mShortCardinal  
#define IDC_M_uint32_t              IDC_mCardinal       
#define IDC_M_uint64_t              IDC_mLongCardinal   
#define IDC_M_int16_t               IDC_mShortInteger   
#define IDC_M_int32_t               IDC_mInteger        
#define IDC_M_int64_t               IDC_mLongInteger    
#define IDC_M_float32_t             IDC_mReal           
#define IDC_M_float64_t             IDC_mLongReal       
#define IDC_M_set_t                 IDC_mSet            
#define IDC_M_addr_t                IDC_mAddress        
#define IDC_M_word_t                IDC_mWord        
#define IDC_M_uint8_t               IDC_mOctet          
#define IDC_M_int8_t                IDC_mChar           
#define IDC_M_nint32_t		    IDC_mNetworkCardinal
#define IDC_M_string_t              IDC$mString

#define IDC_U_bool_t                IDC_uBoolean        
#define IDC_U_uint16_t              IDC_uShortCardinal  
#define IDC_U_uint32_t              IDC_uCardinal       
#define IDC_U_uint64_t              IDC_uLongCardinal   
#define IDC_U_int16_t               IDC_uShortInteger   
#define IDC_U_int32_t               IDC_uInteger        
#define IDC_U_int64_t               IDC_uLongInteger    
#define IDC_U_float32_t             IDC_uReal           
#define IDC_U_float64_t             IDC_uLongReal       
#define IDC_U_set_t                 IDC_uSet            
#define IDC_U_addr_t                IDC_uAddress        
#define IDC_U_word_t                IDC_uWord        
#define IDC_U_uint8_t               IDC_uOctet          
#define IDC_U_int8_t                IDC_uChar           
#define IDC_U_nint32_t		    IDC_uNetworkCardinal
#define IDC_U_string_t              IDC$uString

#define IDC_F_bool_t                IDC_fBoolean        
#define IDC_F_uint16_t              IDC_fShortCardinal  
#define IDC_F_uint32_t              IDC_fCardinal       
#define IDC_F_uint64_t              IDC_fLongCardinal   
#define IDC_F_int16_t               IDC_fShortInteger   
#define IDC_F_int32_t               IDC_fInteger        
#define IDC_F_int64_t               IDC_fLongInteger    
#define IDC_F_float32_t             IDC_fReal           
#define IDC_F_float64_t             IDC_fLongReal       
#define IDC_F_set_t                 IDC_fSet            
#define IDC_F_addr_t                IDC_fAddress        
#define IDC_F_word_t                IDC_fWord        
#define IDC_F_uint8_t               IDC_fOctet          
#define IDC_F_int8_t                IDC_fChar           
#define IDC_F_nint32_t		    IDC_fNetworkCardinal
#define IDC_F_string_t              IDC_fString



#endif /* __IDCMacros_h__ */

