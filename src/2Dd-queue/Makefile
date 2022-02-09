ROOT = ../..

BINS = $(BINDIR)/2Dd-queue

include $(ROOT)/common/Makefile.common

TEST_FILE = ../tests/2Dd-simple_test.c
ifeq ($(TEST), throughput-over-time)
	TEST_FILE = test-simple-over-time.c
endif

PROF = $(ROOT)/src
TEST_FLAG = -DQUEUE

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dd-queue.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dd-queue.o 2Dd-queue.c

test.o: 2Dd-queue.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE) $(TEST_FLAG)

main: measurements.o ssalloc.o  2Dd-queue.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dd-queue.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
