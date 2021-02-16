CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

Mycc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): Mycc.h

test: Mycc
	./test.sh

clean:
	rm -f Mycc *.o *~ tmp*

.PHONY: test clean