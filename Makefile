all: shell

shell: run_shell.c lex.yy.c
	gcc -o shell run_shell.c lex.yy.c -lfl

lex.yy.c: lex.c
	flex lex.c

clean:
	rm shell lex.yy.c
