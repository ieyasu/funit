CC = gcc
FC = gfortran
CFLAGS = -g -Wall -std=c99 -D_POSIX_C_SOURCE=2
FFLAGS = -g -Wall
LFLAGS =

OBJS = funit.o parse_suite.o generate_code.o

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.F90.o:
	$(FC) $(FFLGS) -c $*.F90 -o $*.o

all: funit funit.o

funit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LFLAGS)

test: test/parser/test_parser

test/parser/test_parser: test/parser/test_parser.o parse_suite.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

funit_fortran_module.h: mod_funit.F90
	ruby ./file2stringvar.rb module_code <mod_funit.F90 >funit_fortran_module.h

clean:
	rm -f *.o *.mod *~ funit test/parser/*.o test/parser/test_parser

# deps
generate_code.o: generate_code.c funit_fortran_module.h
