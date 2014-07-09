CC = cc
FC = gfortran
CFLAGS = -g -Wall -std=c99 #-D_POSIX_C_SOURCE=2 -D_BSD_SOURCE
LFLAGS = 

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

all: test_util

test_util: test_util.o ../../util.o
	$(CC) -o $@ $^ $(LFLAGS) && ./$@
