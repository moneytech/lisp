
all: test

build: build/lisp

test: build
	./test_lisp "()" "()"
	./test_lisp "1" "1"
	./test_lisp "(head (quote (1)))" "1"
	./test_lisp "(tail (quote (1)))" "()"
	./test_lisp "(tail (quote (1 2)))" "(2)"
	./test_lisp "(let (a 1) a)" "(1)"

build/lisp: lisp.c Makefile
	mkdir -p build
	gcc -ggdb -Wall -o build/lisp lisp.c

clean: 
	rm -f build

run: build/lisp
	build/lisp


.PHONY: run clean test build