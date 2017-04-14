/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/salloc.h
**
** FUNCTIONAL DESCRIPTION:
** 
**      Macros for dealing with stretches. 
** 
** ENVIRONMENT: 
**
**      Wherever.
** 
** FILE :	salloc.h
** CREATED :	Tue Oct 28 1997
** AUTHOR :	Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ID : 	$Id: salloc.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _SALLOC_H_
#define _SALLOC_H_

#include <Pervasives.ih>
#include <autoconf/memsys.h>

/* Just for syntactic sugar, define the below */
#define Stretch_Rights_None ((Stretch_Rights)0U)


#ifdef CONFIG_MEMSYS_EXPT
#include <StretchAllocatorF.ih>
#include <StretchDriver.ih>

#define STR_NEW(_size)					\
({							\
    Stretch_clp _str;					\
							\
    _str = StretchAllocator$New(Pvs(salloc), _size, 	\
				Stretch_Rights_None); 	\
    StretchDriver$Bind(Pvs(sdriver), _str, PAGE_WIDTH);	\
    _str;						\
})

#define STR_NEW_SALLOC(_salloc, _size)			\
({							\
    Stretch_clp _str;					\
							\
    _str = StretchAllocator$New(_salloc, _size, 	\
				Stretch_Rights_None); 	\
    StretchDriver$Bind(Pvs(sdriver), _str, PAGE_WIDTH);	\
    _str;						\
})

#define STR_NEW_DRIVER(_size, _sdriver)			\
({							\
    Stretch_clp _str;					\
							\
    _str = StretchAllocator$New(Pvs(salloc), _size, 	\
				Stretch_Rights_None);	\
    StretchDriver$Bind(_sdriver, _str, PAGE_WIDTH);	\
    _str;						\
})

#define STR_NEWLIST(_sizes)					\
({								\
    StretchAllocator_StretchSeq *_strs;				\
    int i, nstrs = SEQ_LEN(_sizes);				\
								\
    _strs = StretchAllocator$NewList(Pvs(salloc), _sizes, 	\
				     Stretch_Rights_None);	\
    for(i=0; i < nstrs; i++)					\
	StretchDriver$Bind(Pvs(sdriver),			\
			   SEQ_ELEM(_strs,i),			\
			   PAGE_WIDTH);				\
    _strs;							\
})

#define STR_NEWLIST_SALLOC(_salloc, _sizes)			\
({								\
    StretchAllocator_StretchSeq *_strs;				\
    int i, nstrs = SEQ_LEN(_sizes);				\
								\
    _strs = StretchAllocator$NewList(_salloc, _sizes, 		\
				     Stretch_Rights_None);	\
    for(i=0; i < nstrs; i++)					\
	StretchDriver$Bind(Pvs(sdriver),			\
			   SEQ_ELEM(_strs,i),			\
			   PAGE_WIDTH);				\
    _strs;							\
})

#define STR_NEWLIST_DRIVER(_sizes, _sdriver)			\
({								\
    StretchAllocator_StretchSeq _strs;				\
    int i, nstrs = SEQ_LEN(_sizes);				\
								\
    _strs = StretchAllocator$NewList(Pvs(salloc), _sizes, 	\
				     Stretch_Rights_None);	\
    for(i=0; i < nstrs; i++)					\
	StretchDriver$Bind(_sdriver,				\
			   SEQ_ELEM(_strs,i),			\
			   PAGE_WIDTH);				\
    _strs;							\
})

#define STR_DESTROY(_str)				\
{							\
    StretchDriver$Remove(Pvs(sdriver), _str);		\
    StretchAllocator$DestroyStretch(Pvs(salloc), _str);	\
}

#define STR_DESTROY_DRIVER(_str, _sdriver)		\
{							\
    StretchDriver$Remove(_sdriver, _str);		\
    StretchAllocator$DestroyStretch(Pvs(salloc), _str);	\
}

#define STR_DESTROY_SALLOC(_salloc, _str)		\
{							\
    StretchDriver$Remove(Pvs(sdriver), _str);		\
    StretchAllocator$DestroyStretch(_salloc, _str);	\
}


#else /* !CONFIG_MEMSYS_EXPT */

#include <StretchAllocator.ih>

#define STR_NEW(_size)				\
({						\
    StretchAllocator$New(Pvs(salloc), _size, 	\
			 Stretch_Rights_None); 	\
})

#define STR_NEW_SALLOC(_salloc, _size)		\
({						\
    StretchAllocator$New(_salloc, _size, 	\
			 Stretch_Rights_None);	\
})

#define STR_NEW_DRIVER(_size, _sdriver)		\
({						\
    StretchAllocator$New(Pvs(salloc), _size, 	\
			 Stretch_Rights_None); 	\
})


#define STR_NEWLIST(_sizes)				\
({							\
    StretchAllocator$NewList(Pvs(salloc), _sizes, 	\
			     Stretch_Rights_None); 	\
})

#define STR_NEWLIST_SALLOC(_salloc, _sizes)		\
({							\
    StretchAllocator$NewList(_salloc, _sizes, 		\
			     Stretch_Rights_None); 	\
})

#define STR_NEWLIST_DRIVER(_sizes, _sdriver)		\
({							\
    StretchAllocator$NewList(Pvs(salloc), _sizes, 	\
			     Stretch_Rights_None); 	\
})


#define STR_DESTROY(_str)				\
{							\
    StretchAllocator$DestroyStretch(Pvs(salloc), _str);	\
}

#define STR_DESTROY_DRIVER(_str)			\
{							\
    StretchAllocator$DestroyStretch(Pvs(salloc), _str);	\
}

#define STR_DESTROY_SALLOC(_salloc, _str)	\
{						\
    StretchAllocator$DestroyStretch(_salloc, _str);	\
}


#endif /* !MEMSYS_EXPT */

/* The below are common to all memory systems */

#define SALLOC_DESTROY(_salloc) 		\
{						\
    StretchAllocator$Destroy(_salloc);		\
}

#define STR_RANGE(_str, _sz)			\
({						\
    Stretch$Range(_str, _sz);			\
})

#define STR_INFO(_str, _sz)			\
({						\
    Stretch$Info(_str, _sz);			\
})


#define STR_SETPROT(_str, _pdom, _prot)		\
{						\
    Stretch$SetProt(_str,_pdom, _prot);		\
}


#define STR_REMPROT(_str, _pdom)		\
{						\
    Stretch$RemProt(_str,_pdom);		\
}

#define STR_SETGLOBAL(_str, _prot)		\
{						\
    Stretch$SetGlobal(_str, _prot);		\
}

#define SALLOC_SETPROT(_salloc, _str, _pdom, _prot)	\
{							\
    Stretch$SetProt(_str,_pdom, _prot);			\
}


#define SALLOC_REMPROT(_salloc, _str, _pdom)	\
{						\
    Stretch$RemProt(_str,_pdom);		\
}

#define SALLOC_SETGLOBAL(_salloc, _str, _prot)	\
{						\
    Stretch$SetGlobal(_str, _prot);		\
}




#endif /* _SALLOC_H_ */
