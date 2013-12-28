
lisp: lisp.c Makefile
	gcc -ggdb -Wall -o lisp lisp.c

clean: 
	rm -f lisp
