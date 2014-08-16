
all: build/lisp

build: build/lisp

build/lisp: lisp.c Makefile
	mkdir -p build
	gcc -ggdb -Wall -o build/lisp lisp.c

clean: 
	rm -f build

test: build/lisp
	valgrind --leak-check=full build/lisp

.PHONY: test clean build
