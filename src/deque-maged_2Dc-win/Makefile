ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/deque-maged_2Dc-win
PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

deque-maged_2Dc-win.o: 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/deque-maged_2Dc-win.o deque-maged_2Dc-win.c

test.o: deque-maged_2Dc-win.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  deque-maged_2Dc-win.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/deque-maged_2Dc-win.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
