
#ifndef _CandidateList_h_
#define _CandidateList_h_

/*
 * Candidates are branches of choices.
 */

#define NULL_CandidateList ((CandidateList_t)0)
struct _CandidateList_t { 
  IdentifierList_t il;
  Type_t           type;
  CandidateList_t  next;
};


extern CandidateList_t CandidateList_Append( CandidateList_t list,
 				     CandidateList_t next );

extern CandidateList_t New_CandidateList( IdentifierList_t l, Type_t t );

#endif /* _CandidateList_h_ */
