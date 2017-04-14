
#ifndef _Args_h_
#define _Args_h_

/*
 * Arguments are like fields, but they have a mode as well
 */


typedef enum { ArgKind_Parameter, ArgKind_Result } ArgKind;

#define NULL_ArgList ((ArgList_t)0)
struct _ArgList_t {
  Field_t f;
  Mode_t  m;
  ArgList_t next;
};

extern ArgList_t New_ArgList( Mode_t mode, Field_t field );
extern ArgList_t ArgList_Append( ArgList_t list, ArgList_t al );

extern void      ArgList_CheckDuplicateIds( ArgList_t al1, ArgList_t al2 );
extern void      ArgList_CheckAnnouncement( ArgList_t al1 );
extern void      ArgList_CheckException   ( ArgList_t al1 );

extern void      ArgList_DumpPython( ArgList_t list, ArgKind kind );

#endif /* _Args_h_ */
