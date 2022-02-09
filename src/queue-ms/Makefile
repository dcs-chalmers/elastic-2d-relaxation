ROOT = ../..

include $(ROOT)/common/Makefile.common

ALGO_O = queue-ms.o
ALGO_C = queue-ms.c

BINS = "$(BINDIR)/queue-ms"
PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

queue-lockfree.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/queue-lockfree.o queue-lockfree.c

$(ALGO_O): queue-lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/$(ALGO_O) $(ALGO_C)

intset.o: queue-lockfree.h queue-ms.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: queue-lockfree.h queue-ms.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o queue-lockfree.o $(ALGO_O) intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/queue-lockfree.o $(BUILDIR)/$(ALGO_O) $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
