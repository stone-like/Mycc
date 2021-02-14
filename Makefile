CFLAGS=-std=c11 -g -static

Mycc: Mycc.c

test: Mycc
	./test.sh

clean:
	rm -f Mycc *.o *~ tmp*

.PHONY: test clean