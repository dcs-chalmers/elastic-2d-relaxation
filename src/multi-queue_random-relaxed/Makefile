ROOT = ../..

ifeq ($(CHOICES),two)
	CFLAGS += -DNUM_CHOICES=2
	BINS = $(BINDIR)/multi-qu_ran2c
else ifeq ($(CHOICES),four)
	CFLAGS += -DNUM_CHOICES=4
	BINS = $(BINDIR)/multi-qu_ran4c
else ifeq ($(CHOICES),eight)
	CFLAGS += -DNUM_CHOICES=8
	BINS = $(BINDIR)/multi-qu_ran8c
else
	BINS = $(BINDIR)/multi-qu_ran
endif

include $(ROOT)/common/Makefile.common

PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

multi-qu_ran.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/multi-qu_ran.o multi-queue_random-relaxed.c

test.o: multi-queue_random-relaxed.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  multi-qu_ran.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/multi-qu_ran.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
