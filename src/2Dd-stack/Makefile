ROOT = ../..

BINS = $(BINDIR)/2Dd-stack


include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src
TEST_FLAG = -DSTACK

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dd-stack.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dd-stack.o 2Dd-stack.c

test.o: 2Dd-stack.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(PROF)/tests/2Dd-simple_test.c $(TEST_FLAG)

main: measurements.o ssalloc.o  2Dd-stack.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dd-stack.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
