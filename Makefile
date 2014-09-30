CC = cc
FC = gfortran
CFLAGS = -g -Wall -std=c99 #-D_POSIX_C_SOURCE=2 -D_BSD_SOURCE
FFLAGS = -g -Wall
LFLAGS =

OBJS = funit.o build_and_run.o config.o parse.o parse_test_file.o generate_code.o

.SUFFIXES:
.SUFFIXES: .o .c .F90

.PHONY: all test clean

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.F90.o:
	$(FC) $(FFLAGS) -c $*.F90 -o $*.o

all: funit funit.o

funit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LFLAGS)

funit_fortran_module.h: mod_funit.F90
	ruby ./file2stringvar.rb module_code <mod_funit.F90 >funit_fortran_module.h


test: test/parser/test_parser test/test_parse_build_rule test/test_util \
	test_codegen test/config/test_config

test_codegen: funit
	cd test/code_gen; ./run.sh

test/parser/test_parser: test/parser/test_parser.o parse_test_file.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

test/config/test_config: config.c test/config/test_config.c parse_build_rule.c parse.o util.o
	$(CC) $(CFLAGS) -o $@ test/config/test_config.c parse_build_rule.o parse.o util.o $(LFLAGS)

test/test_parse_build_rule: test/test_parse_build_rule.c parse_build_rule.c config.o parse.o util.o
	$(CC) $(CFLAGS) -o $@ test/test_parse_build_rule.c config.o parse.o util.o $(LFLAGS)

test/test_util: test/test_util.c util.c
	$(CC) $(CFLAGS) -o $@ test/test_util.c $(LFLAGS)

clean:
	rm -f *.o *.mod *~ funit test/parser/*.o test/parser/test_parser

# deps
generate_code.o: generate_code.c funit_fortran_module.h
