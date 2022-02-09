ROOT = ../..

include $(ROOT)/common/Makefile.common

ifeq ($(TEST), switch-middle)
	TEST_FILE = test-switch-middle.c
else ifeq ($(TEST), changing-throughput-over-time)
	TEST_FILE = many-switches-over-time.c
else ifeq ($(TEST), throughput-over-time)
	TEST_FILE = test-simple-over-time.c
else ifeq ($(TEST), simple-controller)
	TEST_FILE = test-simple-controller.c
	CFLAGS += -DELASTIC_CONTROLLER
else ifeq ($(TEST), diff-depths)
	TEST_FILE = test-simple-diff-depths.c
	CFLAGS += -DDIFF_DEPTHS
else ifeq ($(TEST), variable-workload)
	TEST_FILE = test-variable-workload.c
	CFLAGS += -DELASTIC_CONTROLLER
else ifeq ($(TEST), variable-workload-static)
	TEST_FILE = test-variable-workload.c
else
	TEST_FILE = test-simple.c
endif

BINS = $(BINDIR)/2Dd-queue_elastic-lpw
PROF = $(ROOT)/src

.PHONY:	all clean

all:	main

measurements.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/measurements.o $(PROF)/measurements.c

ssalloc.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ssalloc.o $(PROF)/ssalloc.c

2Dd-queue_elastic.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/2Dd-queue_elastic.o 2Dd-queue_elastic.c

test.o: 2Dd-queue_elastic.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o $(TEST_FILE)

main: measurements.o ssalloc.o  2Dd-queue_elastic.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/measurements.o $(BUILDIR)/ssalloc.o $(BUILDIR)/2Dd-queue_elastic.o  $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
