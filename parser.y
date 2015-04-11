%{
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "dosh.h"
%}

%defines
%union {
	char *word;
	struct simple_cmd *simple_cmd;
	struct pipe_cmd *pipe_cmd;
	struct list_cmd *list_cmd;
	struct unit unit;
	struct redir redir;
	enum redir_kind redir_kind;
	enum list_op list_op;
}

%token <word> WORD
%token AMP PIPE AND OR FROM INTO APPEND NEWLINE SEMI

%type <simple_cmd> simple_cmd
%type <pipe_cmd> pipe_cmd
%type <list_cmd> list_cmd
%type <unit> unit
%type <redir_kind> redir_indicator
%type <redir> redir
%type <list_op> list_op

%%

cmd_list:
	| cmd_list full_cmd

sep: NEWLINE
   | SEMI

full_cmd: sep
	| list_cmd sep		{ run_cmd($1, 1); }
	| list_cmd AMP sep	{ run_cmd($1, 0); }
	| error sep		{ yyerrok; }

simple_cmd: unit
	{
		$$ = calloc(1, sizeof(struct simple_cmd));
		$$->argv = malloc(($$->cap = 8) * sizeof(char *));
		add_unit($$, &$1);
	}
	  | simple_cmd unit
	{
		$$ = $1;
		add_unit($$, &$2);
	}

redir_indicator: FROM	{ $$ = REDIR_FROM; }
	       | INTO	{ $$ = REDIR_INTO; }
	       | APPEND	{ $$ = REDIR_APPEND; }

redir: redir_indicator WORD	{ $$.kind = $1; $$.word = $2; }

unit: WORD	{ try_expand($1, &$$); }
    | redir
	{
		switch ($1.kind) {
		case REDIR_INTO:
			$$.kind = U_INTO;
			break;
		case REDIR_FROM:
			$$.kind = U_FROM;
			break;
		case REDIR_APPEND:
			$$.kind = U_APPEND;
		}
		$$.word = $1.word;
	}

pipe_cmd: simple_cmd
	{
		$$ = calloc(1, sizeof *$$);
		$$->leader = $1;
	}
	| pipe_cmd PIPE simple_cmd
	{
		$$ = calloc(1, sizeof *$$);
		$$->leader = $3;
		$$->rest = $1;
	}

list_cmd: pipe_cmd
	{
		$$ = calloc(1, sizeof *$$);
		$$->kind = LIST_UNIT;
		$$->tail = $1;
	}
	| list_cmd list_op pipe_cmd
	{
		$$ = calloc(1, sizeof *$$);
		$$->kind = $2;
		$$->body = $1;
		$$->tail = $3;
	}

list_op: AND	{ $$ = LIST_AND; }
       | OR	{ $$ = LIST_OR; }
       | AMP	{ $$ = LIST_AMP; }
%%
extern FILE *yyin;
extern bool from_file;
int
main(int argc, char **argv)
{
	if (argc>1) {
		yyin = fopen(argv[1], "r");
		if (!yyin)
			err(1, "%s", argv[1]);
		from_file = 1;
	}
	init();
	yyparse();
	return 0;
}
