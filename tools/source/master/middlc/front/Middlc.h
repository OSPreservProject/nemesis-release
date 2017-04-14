
#ifndef _Middlc_h_
#define _Middlc_h_

/*
 * Main header file for Middlc.
 */

#define PY(x) printf x 

#define new(x) ((x##_t)malloc(sizeof(struct _##x##_t)))
#define TYPE_DECL(x) typedef struct _##x##_t *##x##_t

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum { False, True } bool_t;

#define MAX_STRING_LENGTH 1024

TYPE_DECL(ArgList);
TYPE_DECL(CandidateList);
TYPE_DECL(Decl);
TYPE_DECL(DeclList);
TYPE_DECL(ExcList);
TYPE_DECL(Field);
TYPE_DECL(FieldList);
TYPE_DECL(Identifier);
TYPE_DECL(IdentifierList);
TYPE_DECL(Interface);
TYPE_DECL(InterfaceList);
TYPE_DECL(Spec);
TYPE_DECL(SymbolTable);
TYPE_DECL(ScopedName);
TYPE_DECL(Type);
TYPE_DECL(TypeList);
TYPE_DECL(FileStack);

#include "FileStack.h"
#include "SymbolTable.h"

#include "Modes.h"
#include "Decls.h"
#include "Types.h"
#include "Args.h"
#include "Candidates.h"
#include "Exceptions.h"
#include "Fields.h"
#include "Identifiers.h"
#include "Interfaces.h"
#include "Specs.h"
#include "ScopedNames.h"
#include "Parser.h"

#include "Globals.h"
#include "Errors.h"
#endif /* _Middlc_h_ */

