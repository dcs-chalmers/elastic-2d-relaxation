ROOT = ../..

include $(ROOT)/common/Makefile.common

ifeq ($(TEST_FILE),test.c)
	TEST_FILE=test_simple.c
endif

ALGO_O = stack-elimination.o
ALGO_C = stack-elimination.c

BINS = "$(BINDIR)/stack-elimination"
PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

stack-lockfree.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/stack-lockfree.o stack-lockfree.c

$(ALGO_O): stack-lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/$(ALGO_O) $(ALGO_C)

intset.o: stack-lockfree.h stack-elimination.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: stack-lockfree.h stack-elimination.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o stack-lockfree.o $(ALGO_O) intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/stack-lockfree.o $(BUILDIR)/$(ALGO_O) $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
