%{
/*
 *	descrip.l
 *	---------
 *
 * $Id: descrip.l 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */
%}

%%

#.*\n			{ /* ignore comments */ }

[ \t\r\n]		{ /* ignore blanks and tabs */ }


"ntsc"			return TK_NTSC;
"module"		return TK_MODULE;
"blob"			return TK_BLOB;
"align"			return TK_ALIGN;
"namespace"		return TK_NAMESPACE;
"program"		return TK_PROGRAM;
"primal"		return TK_PRIMAL;
"end"			return TK_END;		/* needed ? */


[0-9]+			{ yylval.num = strtol(yytext,NULL,0); return TK_NUM; }

[0-9]+ns		{ yylval.time = strtol(yytext,NULL,0);
				return TK_TIME; }
[0-9]+us		{ yylval.time = strtol(yytext,NULL,0) * 1000;
				return TK_TIME; }
[0-9]+ms		{ yylval.time = strtol(yytext,NULL,0) * 1000000;
				return TK_TIME; }
[0-9]+s			{ yylval.time = strtol(yytext,NULL,0) * 1000000000;
				return TK_TIME; }

[0-9]+[Kk]b?		{ yylval.size = strtol(yytext,NULL,0) * 1024;
				return TK_SIZE; }
[0-9]+[Mm]b?		{ yylval.size = strtol(yytext,NULL,0) * 1048576;
				return TK_SIZE; }

[A-Za-z_][A-Za-z_0-9]*	{ yylval.string = strdup(yytext); return TK_STRING; }

[-A-Za-z0-9_./>]*		{ yylval.string = strdup(yytext); return TK_PATH; }


.  			return yytext[0];
