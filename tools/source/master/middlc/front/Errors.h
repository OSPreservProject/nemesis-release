
#ifndef _Errors_h_
#define _Errors_h_

/*
 * Error handling system for the front end.
 */

extern void Error_Simple( char *msg );
extern void Error_Symbol( char *msg, Identifier_t name );
extern void Error_Symbols( char *msg, Identifier_t i1, Identifier_t i2 );
extern void Error_Scoped( char *msg, ScopedName_t sn );
extern void Error_String( char *msg, char *str );
extern void Error_Fatal( char *msg );
extern void Error_Bug( char *msg );


#endif /* _Errors_h_ */
