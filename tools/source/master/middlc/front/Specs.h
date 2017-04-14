
#ifndef _Specs_h_
#define _Specs_h_

/*
 * Specs are the top-level structures returned by the parser,
 */

typedef enum { IntfSpec } SpecType_t;

#define NULL_Spec ((Spec_t)0)
struct _Spec_t {
  Interface_t intf;
};

extern Spec_t New_IntfSpec( Interface_t intf );

extern void Spec_DumpPython( Spec_t );
extern void Spec_MakeDepend( Spec_t );
extern void Spec_DumpBannerInit( );

#endif _Specs_h_
