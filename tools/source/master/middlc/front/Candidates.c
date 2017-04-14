
/*
 * Code for manipulating candidate lists.
 */

#include "Middlc.h"
#include "Candidates.h"

/*
 * Add a new candidate to a list, creating the list if necessary.
 */
CandidateList_t CandidateList_Append( CandidateList_t list,
 				     CandidateList_t next )
{
  if ( list ) {
    CandidateList_t l;

    for( l=list; l->next; l=l->next );
    l->next = next;
    return list;
  } else {
    return next;
  }
}

/*
 * Create a new candidate list from scratch
 */
CandidateList_t New_CandidateList( IdentifierList_t l, Type_t t )
{
  CandidateList_t cl = new(CandidateList);

  cl->il = l;
  cl->type = t;
  cl->next = NULL_CandidateList;
  return cl;
}
