CC = gcc
CFLAGS = -g -Wall -std=c99
LFLAGS =

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

test_parser: test_parser.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

generate_code: generate_code.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

clean:
	rm -f *.o *~
