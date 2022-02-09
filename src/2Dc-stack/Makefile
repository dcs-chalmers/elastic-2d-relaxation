ROOT = ../..

BINS = $(BINDIR)/2Dc-stack


include $(ROOT)/common/Makefile.common

ifeq ($(TEST), throughput-over-time)
	TEST_FILE = test-simple-over-time.c
endif

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dc-stack.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dc-stack.o 2Dc-stack.c

test.o: 2Dc-stack.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  2Dc-stack.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dc-stack.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
