ROOT = ../..

BINS = $(BINDIR)/queue-lcrq

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

test.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

xxhash.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/xxhash.o xxhash.c

hzdptr.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/hzdptr.o hzdptr.c

lcrq.o: hzdptr.o xxhash.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lcrq.o lcrq.c

main: measurements.o lcrq.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/hzdptr.o $(BUILDIR)/xxhash.o $(BUILDIR)/lcrq.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
