CC = gcc
CFLAGS = -g -Wall -std=c99
LFLAGS =

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

all:

test_parser: test_parser.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

generate_code: generate_code.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

funit_fortran_module.h: funit.F90
	ruby ./file2stringvar.rb module_code <funit.F90 >funit_fortran_module.h

clean:
	rm -f *.o *~

# deps
generate_code.o: generate_code.c funit_fortran_module.h
