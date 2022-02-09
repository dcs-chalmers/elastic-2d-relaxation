ROOT = ../..

BINS = $(BINDIR)/queue-wf

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

# Do we actually need anything from here?
measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

test.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

wfqueue.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/wfqueue.o wfqueue.c

main: measurements.o wfqueue.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/wfqueue.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
