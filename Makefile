dosh: lexer.o parser.o dosh.o builtins.o history.o wildcard.o
	cc $^ -lfl -o $@

lexer.o: lexer.l dosh.h y.tab.h

dosh.o: dosh.c dosh.h builtins.h

y.tab.h: parser.y

builtins.o: builtins.c builtins.h

builtins.c: builtins.gperf
	gperf -tT $< > $@

wildcard.o: wildcard.c dosh.h

.INTERMEDIATE: builtins.c
