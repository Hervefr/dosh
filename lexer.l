%{
#include <stdbool.h>
#include <string.h>
#include "dosh.h"
#include "y.tab.h"
static size_t
myinput(char *s, int size)
{
	static char *last;
	prompt();
	if (fgets(s, size, yyin)) {
		if (last)
			history_add(last);
		last = strdup(s);
		return strlen(s);
	}
	return 0;
}
#define YY_INPUT(buf, result, maxsize) result = myinput(buf, maxsize)
%}
%%
"&"	{ return AMP; }
"|"	{ return PIPE; }
"&&"	{ return AND; }
"||"	{ return OR; }
"<"	{ return FROM; }
">"	{ return INTO; }
">>"	{ return APPEND; }
\n	{ return NEWLINE; }
";"	{ return SEMI; }
[ \t]	{}
[*-:?-Z_a-z]+	{ yylval.word = strdup(yytext); return WORD; }
.	{ fprintf(stderr, "unrecognized character %s\n", yytext); }
%%
