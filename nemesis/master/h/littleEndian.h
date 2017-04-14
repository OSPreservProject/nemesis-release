/*
 *    this is a little endian machine
 */

#if defined(__GNUC__) && (defined(arm) || defined(arm32)) && defined(__OPTIMIZE__)

/* Dont need __volatile__ because we dont mind being optimised
 * out. Using special "0" to indicate we are constrained to using the
 * same register.  The & tells it not to put in and tmp in the same
 * register. This must be an inline function rather than a macro
 * because gcc does not permit asm to be used inside the initialiser
 * of a declaration of a local variable of a function. */

extern __inline__ uint32_t ntoh16(const uint32_t in) __attribute__ ((const));
extern __inline__ uint32_t ntoh16(const uint32_t in)
{
    uint32_t tmp, out;
    __asm__ (" and %1, %2, # 0x000000ff ; "	/* %1 = 000000dd */
	     " orr %0, %1, %2, lsl #16 ; "	/* %0 = ccdd00dd */
	     " mov %0, %0, ror #8 ; "		/* %0 = ddccdd00 */
	     " mov %0, %0, lsr #16 "		/* %0 = 0000ddcc */
	     : "=r" (out) , "=&r" (tmp) : "r" (in) );
    return out;
}

extern __inline__ uint32_t ntoh32(const uint32_t in) __attribute__ ((const));
extern __inline__ uint32_t ntoh32(const uint32_t in)
{
    uint32_t tmp, out;
    __asm__ (" eor %1, %2, %2, ror #16 ; "
	     " bic %1, %1, # 0x00ff0000 ; "
	     " mov %0, %2, ror #8 ; "
	     " eor %0, %0, %1, lsr #8 "
	     : "=r" (out) , "=&r" (tmp) : "r" (in) );
    return out;
}

#else

/* We're careful to evaluate arguments once only. */

#define ntoh16(_s) \
  ({ uint16_t s = _s;	((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)); })

#define ntoh32(_l) \
  ({ uint32_t l = _l;	(( ((l) & 0x000000ff) << 24 ) | \
			( (((l) & 0xff000000) >> 24 ) & 0x000000ff) | \
			( (((l) & 0x00ff0000) >> 8 ) & 0x0000ff00)  | \
			( ((l) & 0x0000ff00) << 8 )); })

#endif

#define hton16(s)	ntoh16(s)
#define hton32(l)	ntoh32(l)

