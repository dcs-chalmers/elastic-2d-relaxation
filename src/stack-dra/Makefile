ROOT = ../..

ifeq ($(CHOICES),two)
	CFLAGS += -DNUM_CHOICES=2
	BINS = $(BINDIR)/stack-2ra
else ifeq ($(CHOICES),four)
	CFLAGS += -DNUM_CHOICES=4
	BINS = $(BINDIR)/stack-4ra
else ifeq ($(CHOICES),eight)
	CFLAGS += -DNUM_CHOICES=8
	BINS = $(BINDIR)/stack-8ra
else
	BINS = $(BINDIR)/stack-1ra
endif

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

stack-dra.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/stack-dra.o stack-dra.c

test.o: stack-dra.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  stack-dra.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/stack-dra.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
