
build/lisp: lisp.c Makefile
    mkdir -p build
	gcc -ggdb -Wall -o lisp lisp.c

clean: 
	rm -f build

run: build/lisp
	build/lisp
