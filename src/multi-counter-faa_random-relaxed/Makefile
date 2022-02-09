ROOT = ../..

BINS = $(BINDIR)/multi-ct-faa_ran


include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

multi-ct-faa_ran.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/multi-ct-faa_ran.o multi-counter-faa_random-relaxed.c

test.o: multi-counter-faa_random-relaxed.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  multi-ct-faa_ran.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/multi-ct-faa_ran.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)