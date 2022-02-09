ROOT = ../..

ifeq ($(CHOICES),two)
	CFLAGS += -DNUM_CHOICES=2
	BINS = $(BINDIR)/queue-2ra
else ifeq ($(CHOICES),four)
	CFLAGS += -DNUM_CHOICES=4
	BINS = $(BINDIR)/queue-4ra
else ifeq ($(CHOICES),eight)
	CFLAGS += -DNUM_CHOICES=8
	BINS = $(BINDIR)/queue-8ra
else
	BINS = $(BINDIR)/queue-1ra
endif

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

queue-dra.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/queue-dra.o queue-dra.c

test.o: queue-dra.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  queue-dra.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/queue-dra.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
