
all: build/lisp

build: build/lisp

build/lisp: lisp.c Makefile
	mkdir -p build
	gcc -ggdb -Wall -o build/lisp lisp.c

clean: 
	rm -f build

run: build/lisp
	build/lisp

.PHONY: run clean build
