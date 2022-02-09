ROOT = ../..

include $(ROOT)/common/Makefile.common

# TEST_FILE = test-simple.c

BINS = $(BINDIR)/2Dd-queue_optimized
PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dd-queue_optimized.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dd-queue_optimized.o 2Dd-queue_optimized.c

test.o: 2Dd-queue_optimized.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  2Dd-queue_optimized.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dd-queue_optimized.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
