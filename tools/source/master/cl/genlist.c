/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
#include <stdio.h>

#include "cluless.h"

#include "stackobj.h"
#include "tables.h"
#include "list.h"
#include "statement.h"
#include "type.h"
#include "spec.h"
#include "decl.h"
#include "dtree.h"
#include "expr.h"
#include "group.h"
#include "parsupport.h"
#include "typesupport.h"


int main (int argc, char *argv[]) {
    int g;
    int f;
    int i;
    int x;
    int count;
    int meta[group_size];
    int kids[5];
    int numkids;
    struct table_format *x0;
    
    void output(void) {
	int j;
	printf("{%d, %d, \"%s\", \"%s\", %d, {", g+1, f ? f+1:0, t_group[g].name, f? x0[f].name : "unique", numkids);
	for (j=0; j<=numkids; j++) {
	    printf("%d", kids[j]);
	    if (j<numkids) printf(",");
	}
	printf("}},\n");
    }

    g = 0;
    printf("struct mastertable_t {\n");
    printf("  int class;\n");
    printf("  int kind;\n");
    printf("  char *classname;\n");
    printf("  char *formname;\n");
    printf("  int num;\n");
    printf("  int childkind[5];\n");
    printf("};\n");
    printf("\n");
    printf("const struct mastertable_t mastertable[] = {\n");
    count =0;
    while (t_group[g].name) {
	meta[g] = count;
	if (t_group[g].x0) {
	    x0 = t_group[g].x0;
	    f = 0;
	    numkids =0;
	    while (x0[f].name) {
		for (i=0; i<5; i++) {
		    x = t_group[g].nature[i];
		    if (x == n_generic) {
			x = x0[f].nature[i];
		    }
		    if (x) numkids = i;
		    if (x>0) x--;
		    kids[i] = x;
		}
		output();
		f++;
		count++;
	    }		       
		
	} else {
	    f = 0;
	    numkids = 0;
	    for (i=0; i<5; i++) {
		x = (int) t_group[g].nature[i];
		if (x>0) x--;
		kids[i] = x;
		if (x) numkids = i;
	    }
	    output();
	    count++;
	}
	g++;
    }
    printf("{0}};\n");
    printf("const int metamastertable[%d] = {", g);
    for (i=0; i<g; i++) {
	printf("%d", meta[i]);
	if (i<g-1) printf(",");
    }
    printf("};\n");
    return 0; 
}
    
    
