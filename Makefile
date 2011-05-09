CC = gcc
CFLAGS = -g -Wall -std=c99
LFLAGS =

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

all: test

test: test/parser/test_parser

test/parser/test_parser: test/parser/test_parser.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

generate_code: generate_code.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

funit_fortran_module.h: funit.F90
	ruby ./file2stringvar.rb module_code <funit.F90 >funit_fortran_module.h

clean:
	rm -f *.o *~ test/parsesr/*.o test/parser/test_parser

# deps
generate_code.o: generate_code.c funit_fortran_module.h
