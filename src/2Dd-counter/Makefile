ROOT = ../..

BINS = $(BINDIR)/2Dd-counter


include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src
TEST_FLAG = -DCOUNTER

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dd-counter.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dd-counter.o 2Dd-counter.c

test.o: 2Dd-counter.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(PROF)/tests/2Dd-simple_test.c $(TEST_FLAG)

main: measurements.o ssalloc.o  2Dd-counter.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dd-counter.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
