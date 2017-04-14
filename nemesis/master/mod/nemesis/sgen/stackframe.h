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
**      mod/nemesis/sgen/stackframe.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Provides an architecture-independent interface to manipulating
**      stack frames. Very similar to var_args, except that it has to
**      work with caller stack frames as well as callee frames and so
**      gets a little more complex
** 
** ID : $Id: stackframe.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
** */

#define ALLOC_CALLER_STACK_FRAME(_sf, _size) \
INIT_CALLER_STACK_FRAME(_sf, _size, GET_STACK_FRAME_SPACE(_size))

#ifdef __IX86__

typedef struct {
    addr_t p, stackbase;
    uint32_t size;
} stack_frame_t;

#define GET_STACK_FRAME_SPACE(_size) alloca(_size)

#define INIT_CALLER_STACK_FRAME(_sf, _size, _memory) \
do { \
    (_sf).p = (_sf).stackbase = (_memory); \
    (_sf).size = (_size);\
} while (0)

#define INIT_CALLEE_STACK_FRAME(_sf, _args) \
                  (_sf).p = (_sf).stackbase = (_args)

#define RESET_STACK_FRAME(_sf)  (_sf).p = (_sf).stackbase;

#define SET_INT32_ARG(_sf, _src) \
do { \
    TRCMUF(eprintf("SF: %p - Setting int32 arg at %p to 0x%08x\n", \
	    &(_sf), (_sf).p, (_src)));\
    *((uint32_t *)((_sf).p))++ = (_src); \
} while(0)

#define SET_INT64_ARG(_sf, _src) \
do { \
    TRCMUF(eprintf("SF: %p - Setting int64 arg at %p to %qx\n", \
	    &(_sf), (_sf).p, (_src)));\
    *((uint64_t *)((_sf).p))++ = (_src); \
} while(0)

#define SET_FLOAT32_ARG(_sf, _src) SET_INT32_ARG((_sf), (_src))

#define SET_FLOAT64_ARG(_sf, _src) SET_INT64_ARG((_sf), (_src))

#define SET_PTR_ARG(_sf, _src) SET_INT32_ARG(_sf, (uint32_t) (_src))

#define GET_INT32_ARG(_sf, _dest) \
do { \
    TRCMUF(eprintf("SF: %p - Getting int32 arg from %p: 0x%08x\n", \
	    &(_sf), (_sf).p, *(uint32_t *)((_sf).p)));\
    *((uint32_t *)(_dest)) = *((uint32_t *)((_sf).p))++; \
} while (0)

#define GET_INT64_ARG(_sf, _dest) \
do { \
    TRCMUF(eprintf("SF: %p - Getting int64 arg from %p: 0x%qx\n", \
	    &(_sf), (_sf).p, *(uint64_t *)((_sf).p))); \
    *((uint64_t *)(_dest)) = *((uint64_t *)((_sf).p))++; \
} while (0)

#define GET_FLOAT32_ARG(_sf, _dest) GET_INT32_ARG((_sf), (_dest))

#define GET_FLOAT64_ARG(_sf, _dest) GET_INT64_ARG((_sf), (_dest))
			
#define GET_PTR_ARG(_sf, _dest) GET_INT32_ARG((_sf), (_dest))

#define SKIP_INT32_ARG(_sf) ({uint32_t *tmp = (_sf).p; (_sf).p += 4; tmp;})
#define SKIP_INT64_ARG(_sf) ({uint64_t *tmp = (_sf).p; (_sf).p += 8; tmp;})
#define SKIP_FLOAT32_ARG(_sf) ({ float32_t *tmp = (_sf).p; (_sf).p += 4; tmp;})
#define SKIP_FLOAT64_ARG(_sf) ({ float64_t *tmp = (_sf).p; (_sf).p += 8; tmp;})
#define SKIP_PTR_ARG(_sf) ({ word_t *tmp = (_sf).p; (_sf).p += sizeof(word_t); tmp;})

#define CALL_STACK_FRAME(_sf, _func, _res) \
	  __asm__("subl %%ebx, %%esp;"\
		  "movl %%esp, %%edi;"\
		  "movl %%ebx, %%ecx;"\
		  "shr  $2, %%ecx;"\
		  ".word 0xa5f3;" /* rep movsd XXX */ \
		  "call *%2;"\
		  "addl %%ebx, %%esp;"\
                  : "=a" (((uint32_t *)_res)[0]), "=d" (((uint32_t *)_res)[1])\
                  : "a" (_func), "b" ((_sf).size), "S" ((_sf).stackbase) \
                  : "edi", "esi", "ecx", "cc" )


#endif

#ifdef __ALPHA__

typedef struct {
    addr_t p, stackbase;
    uint64_t size;
} stack_frame_t;

#define GET_STACK_FRAME_SPACE(_size) alloca((_size) + 48)

#define INIT_CALLER_STACK_FRAME(_sf, _size, _memory) \
do { \
    (_sf).p = (_sf).stackbase = (_memory) + 48; \
    (_sf).size = (_size);\
} while (0)

#define INIT_CALLEE_STACK_FRAME(_sf, _args) \
    (_sf).p = (_sf).stackbase = (_args);\

#define RESET_STACK_FRAME(_sf) \
{ (_sf).p = (_sf).stackbase;  }

#define SET_INT64_ARG(_sf, _src) \
do { \
    TRCMUF(eprintf("SF: %p - Setting int64 arg at %p to %qx\n", \
	    &(_sf), (_sf).p, (_src)));\
    *((uint64_t *)((_sf).p))++ = (_src); \
} while(0)

#define SET_INT32_ARG(_sf, _src) SET_INT64_ARG(_sf, _src)

#define SET_FLOAT64_ARG(_sf, _src) \
do { \
    TRCMUF(eprintf("SF: %p - Setting float64 arg at %p\n", \
	    &(_sf), (_sf).p - 48)); \
    *((float64_t *) ((((_sf).p)++) - 48) = (_src); \
} while(0)

#define SET_FLOAT32_ARG(_sf, _src) SET_FLOAT64_ARG(_sf, _src)

#define SET_PTR_ARG(_sf, _src) SET_INT64_ARG(_sf, ((uint64_t)(_src)))

#define GET_INT64_ARG(_sf, _dest) \
do { \
    TRCMUF(eprintf("SF: %p - Getting int64 arg from %p: 0x%qx\n", \
	    &(_sf), (_sf).p, *(uint64_t *)((_sf).p))); \
    *((uint64_t *)(_dest)) = *((uint64_t *)((_sf).p))++; \
} while (0)

#define GET_INT32_ARG(_sf, _dest) GET_INT64_ARG(_sf, _dest)

#define GET_FLOAT64_ARG(_sf, _dest) \
do { \
    TRCMUF(eprintf("SF: %p - Getting float64 arg from %p\n", \
		   &(_sf), (_sf).p - 48)); \
    *((float64_t *)(_dest)) = *((float64_t *) ((((_sf).p)++) - 48)); \
} while(0)

#define GET_FLOAT32_ARG(_sf, _dest) GET_FLOAT64_ARG(_sf, _dest)

#define GET_PTR_ARG(_sf, _dest) GET_INT64_ARG(_sf, _dest)

#define SKIP_INT64_ARG(_sf) ({uint64_t *tmp = (_sf).p; (_sf).p += 8; tmp;})
#define SKIP_INT32_ARG(_sf) SKIP_INT64_ARG(_sf)
#define SKIP_FLOAT64_ARG(_sf) (ntsc_dbgstop(), NULL)
#define SKIP_FLOAT32_ARG(_sf) SKIP_FLOAT64_ARG(_sf)
#define SKIP_PTR_ARG(_sf) SKIP_INT64_ARG(_sf)

#define CALL_STACK_FRAME(_sf, _func, _res) \
    __asm__(   \
            "bis  $30, $30,   $9   # save SP
             bis  $29, $29,   $10  # save GP, 'cos gcc won't
             subq $30, %2;         # Expand stack  
             bis  $30, $30,   $3
             ldq  $16, 0(%3);      # load args
             ldq  $17, 8(%3);
             ldq  $18, 16(%3);
             ldq  $19, 24(%3);
             ldq  $20, 32(%3);
             ldq  $21, 40(%3);
             lda  %3,  48(%3);
             
	     beq  %2,  2f;         # args left == 0?   
1:	     ldq  $5,  0(%3);      # *frame++ = *arg++
	     lda  %3,  8(%3);
	     stq  $5,  0($3);
	     lda  $3,  8($3);
             subq %2,  8, %2
	     bne  %2,  1b;
2:           bis  %1,  %1,    $27;
             jsr  $26, ($27);
             bis  $0,  $0,    %0
             bis  $9,  $9,    $30  # restore sp
             bis  $10, $10,   $29  # restore gp" : \
				"=r" (*(uint64_t *)(_res)) : \
				"r" (  func ), \
				"r" MAX(0, ((int64_t)((_sf).p - (_sf).stackbase)) - ((int64_t)(6 * sizeof(uint64_t)))), \
				"r" (( _sf ).stackbase) :\
				 "$3", "$5", "$9", "$10", "$16", "$17", "$18", "$19", "$20", "$21", "$26", "$27") ;

#endif

#ifdef __ARM__

typedef struct {
    addr_t p, stackbase;
    uint32_t size;
} stack_frame_t;

#define GET_STACK_FRAME_SPACE(_size) alloca(_size)

#define INIT_CALLER_STACK_FRAME(_sf, _size, _memory) \
do { \
    (_sf).p = (_sf).stackbase = (_memory); \
    (_sf).size = (_size);\
} while (0)

#define INIT_CALLEE_STACK_FRAME(_sf, _args) \
    (_sf).p = (_sf).stackbase = (_args);\


#define RESET_STACK_FRAME(_sf) \
{ (_sf).p = (_sf).stackbase;  }

#define SET_INT64_ARG(_sf, _src) \
do { \
    TRCMUF(eprintf("SF: %p - Setting int64 arg at %p to %qx\n", \
	    &(_sf), (_sf).p, (_src)));\
    *((uint64_t *)((_sf).p))++ = (_src); \
} while(0)

#define SET_INT32_ARG(_sf, _src) \
  do { \
    TRCMUF(eprintf("SF: %p - Setting int32 arg at %p to 0x%08x\n", \
		   &(_sf), (_sf).p, (_src)));\
    *((uint32_t *)((_sf).p))++ = (_src); \
} while(0)

#define SET_FLOAT64_ARG(_sf, _src) ntsc_dbgstop()

#define SET_FLOAT32_ARG(_sf, _src) ntsc_dbgstop()

#define SET_PTR_ARG(_sf, _src) SET_INT32_ARG(_sf, ((uint32_t) (_src)))

#define GET_INT64_ARG(_sf, _dest) \
do { \
    TRCMUF(eprintf("SF: %p - Getting int64 arg from %p: 0x%qx\n", \
	    &(_sf), (_sf).p, *(uint64_t *)((_sf).p))); \
    *((uint64_t *)(_dest)) = *((uint64_t *)((_sf).p))++; \
} while (0)

#define GET_INT32_ARG(_sf, _dest) \
  do { \
    TRCMUF(eprintf("SF: %p - Getting int32 arg from %p: 0x%08x\n", \
	   &(_sf), (_sf).p, *(uint32_t *)((_sf).p)));\
    *((uint32_t *)(_dest)) = *((uint32_t *)((_sf).p))++; \
} while (0)

#define GET_FLOAT64_ARG(_sf, _dest) ntsc_dbgstop()

#define GET_FLOAT32_ARG(_sf, _dest) ntsc_dbgstop()

#define GET_PTR_ARG(_sf, _dest) GET_INT32_ARG(_sf, _dest)

#define SKIP_INT64_ARG(_sf) ({uint64_t *tmp = (_sf).p; (_sf).p += 8; tmp;})
#define SKIP_INT32_ARG(_sf) ({uint32_t *tmp = (_sf).p; (_sf).p += 4; tmp;})
#define SKIP_FLOAT64_ARG(_sf) (ntsc_dbgstop(), NULL)
#define SKIP_FLOAT32_ARG(_sf) (ntsc_dbgstop(), NULL)
#define SKIP_PTR_ARG(_sf) SKIP_INT32_ARG(_sf)

#define CALL_STACK_FRAME(_sf, _func, _res) \
      __asm__(\
              "mov r4, sp @ save stackpointer
               teq %3, #0 @ check to see if we need to increase stack
               beq 2f @ if we don`t then jump skip stack stuff

               sub sp, sp, %3 @ otherwise increase the stack
               mov r1, sp @ copy the sp pointer to register
               mov r3, %4 @ need a copy of arg4
               add r3, r3, #16 @ increment it to the right place
1:             ldr r2, [r3], #4 @ load arg into register 
               str r2, [r1], #4 @ stor arg in stack 
               subs %3, %3, #4 @ reduce number of args 
               bne 1b @ if there are still args then loop

2:             ldmia %4!, {r0-r3} @ load args into registers
               mov r14, r15 @ store PC
               mov r15, %2 @ and jump
               mov %0, r0 @ put in results
               mov %1, r1
               mov sp, r4 @ restore stackpointer" : \
	      "=r" (((uint32_t *)_res)[0]), "=r" (((uint32_t *)_res)[1]) : \
	      "r" ( _func ), \
	      "r" MAX(0, ((int32_t)((_sf).p - (_sf).stackbase)) - ((int32_t)(4 * sizeof(uint32_t)))), \
	      "r" (( _sf ).stackbase) :\
	      "r0", "r1", "r2", "r3", "r4", "r12", "r14", "cc"); 

#endif /* __ARM__ */
