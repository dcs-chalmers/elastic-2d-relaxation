ROOT = ../..

BINS = $(BINDIR)/2Dc-stack_elastic-lpw

include $(ROOT)/common/Makefile.common

ifeq ($(TEST), switch-middle)
	TEST_FILE = test-switch-middle.c
else ifeq ($(TEST), throughput-over-time)
	TEST_FILE = test-simple-rt-benchmark.c
else ifeq ($(TEST), changing-throughput-over-time)
	TEST_FILE = many-switches-over-time.c
else
	TEST_FILE = test-simple.c
endif

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dc-stack_elastic.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dc-stack_elastic.o 2Dc-stack_elastic.c

test.o: 2Dc-stack_elastic.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  2Dc-stack_elastic.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dc-stack_elastic.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
