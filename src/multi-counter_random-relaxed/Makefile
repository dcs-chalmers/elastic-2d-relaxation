ROOT = ../..

ifeq ($(CHOICES),two)
	CFLAGS += -DNUM_CHOICES=2
	BINS = $(BINDIR)/multi-ct_ran2c
else ifeq ($(CHOICES),four)
	CFLAGS += -DNUM_CHOICES=4
	BINS = $(BINDIR)/multi-ct_ran4c
else ifeq ($(CHOICES),eight)
	CFLAGS += -DNUM_CHOICES=8
	BINS = $(BINDIR)/multi-ct_ran8c
else
	BINS = $(BINDIR)/multi-ct_ran
endif

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

multi-ct_ran.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/multi-ct_ran.o multi-counter_random-relaxed.c

test.o: multi-counter_random-relaxed.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  multi-ct_ran.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/multi-ct_ran.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)