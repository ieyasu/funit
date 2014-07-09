CC = cc
FC = gfortran
CFLAGS = -g -Wall -std=c99 #-D_POSIX_C_SOURCE=2 -D_BSD_SOURCE
FFLAGS = -g -Wall
LFLAGS =

OBJS = funit.o build_and_run.o config.o parse.o parse_test_file.o generate_code.o util.o
TCD = test/code
TEST_PROGS = test/parser/test_parser $(TCD)/test_util

.SUFFIXES: .o .c .F90 .f90

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.F90.o:
	$(FC) $(FFLAGS) -c $*.F90 -o $*.o

all: funit funit.o

funit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LFLAGS)

test: test/parser/test_parser

test/parser/test_parser: test/parser/test_parser.o parse_test_file.o parse.o util.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

funit_fortran_module.h: mod_funit.F90
	ruby ./file2stringvar.rb module_code <mod_funit.F90 >funit_fortran_module.h

test: funit test_code
	cd test/code_gen; ./run.sh # functional test of code generation

# unit test funit's code
test_code: $(OBJS)
	cd test/code && $(MAKE)
	@echo
	@echo "many warnings should have appeared above; this is normal"


clean: testclean
	rm -f *.o *.mod *~ funit

testclean:
	rm -f $(TEST_PROGS) test/*/*.o

# deps
generate_code.o: generate_code.c funit_fortran_module.h
