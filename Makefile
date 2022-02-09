BENCHS = src/stack-dra src/queue-dra src/queue-lcrq src/queue-ms src/queue-wf src/queue-k-segment src/stack-elimination src/stack-k-segment src/stack-treiber src/2Dc-deque src/2Dd-deque src/2Dc-counter src/2Dd-counter src/2Dc-stack src/2Dc-stack_optimized src/2Dc-stack_elastic-lpw src/2Dd-stack src/multi-stack_random-relaxed src/multi-queue_random-relaxed src/multi-counter-faa_random-relaxed src/multi-counter_random-relaxed  src/2Dd-queue src/2Dd-queue_optimized src/2Dd-queue_elastic-lpw src/2Dd-queue_elastic-law


.PHONY:	clean $(BENCHS)

all:
	$(MAKE)  2D multi_ran external_queues external_stacks

2Dd-queue:
	$(MAKE) src/2Dd-queue
2Dd-queue_optimized:
	$(MAKE) src/2Dd-queue_optimized
2Dd-queue_elastic-lpw:
	$(MAKE) src/2Dd-queue_elastic-lpw
2Dd-queue_elastic-law:
	$(MAKE) src/2Dd-queue_elastic-law
multi-qu_ran:
	$(MAKE) src/multi-queue_random-relaxed
multi-qu_ran2c:
	$(MAKE) "CHOICES=two" src/multi-queue_random-relaxed
multi-qu_ran4c:
	$(MAKE) "CHOICES=four" src/multi-queue_random-relaxed
multi-qu_ran8c:
	$(MAKE) "CHOICES=eight" src/multi-queue_random-relaxed
queue-1ra:
	$(MAKE) src/queue-dra
queue-2ra:
	$(MAKE) "CHOICES=two" src/queue-dra
queue-4ra:
	$(MAKE) "CHOICES=four" src/queue-dra
queue-8ra:
	$(MAKE) "CHOICES=eight" src/queue-dra


2Dd-deque:
	$(MAKE) src/2Dd-deque
2Dc-deque:
	$(MAKE) src/2Dc-deque

stack-treiber:
	$(MAKE) src/stack-treiber
stack-elimination:
	$(MAKE) src/stack-elimination
stack-k-segment:
	$(MAKE) src/stack-k-segment
multi-st_ran:
	$(MAKE) src/multi-stack_random-relaxed
multi-st_ran2c:
	$(MAKE) "CHOICES=two" src/multi-stack_random-relaxed
multi-st_ran4c:
	$(MAKE) "CHOICES=four" src/multi-stack_random-relaxed
multi-st_ran8c:
	$(MAKE) "CHOICES=eight" src/multi-stack_random-relaxed
stack-1ra:
	$(MAKE) src/stack-dra
stack-2ra:
	$(MAKE) "CHOICES=two" src/stack-dra
stack-4ra:
	$(MAKE) "CHOICES=four" src/stack-dra
stack-8ra:
	$(MAKE) "CHOICES=eight" src/stack-dra
2Dc-stack:
	$(MAKE) -C src/2Dc-stack main
2Dc-stack_optimized:
	$(MAKE) -C src/2Dc-stack_optimized main
2Dc-stack_elastic-lpw:
	$(MAKE) src/2Dc-stack_elastic-lpw
2Dd-stack:
	$(MAKE) src/2Dd-stack

queue-lcrq:
	$(MAKE) src/queue-lcrq
queue-ms:
	$(MAKE) src/queue-ms
queue-wf:
	$(MAKE) src/queue-wf
queue-k-segment:
	$(MAKE) src/queue-k-segment
multi-ct-faa_ran:
	$(MAKE) src/multi-counter-faa_random-relaxed
multi-ct_ran:
	$(MAKE) src/multi-counter_random-relaxed
multi-ct_ran2c:
	$(MAKE) "CHOICES=two" src/multi-counter_random-relaxed
multi-ct_ran4c:
	$(MAKE) "CHOICES=four" src/multi-counter_random-relaxed
multi-ct_ran8c:
	$(MAKE) "CHOICES=eight" src/multi-counter_random-relaxed
2Dc-counter:
	$(MAKE) src/2Dc-counter
2Dd-counter:
	$(MAKE) src/2Dd-counter


2D: 2Dc 2Dd
2Dc: 2Dc-deque 2Dc-counter 2Dc-stack 2Dc-stack_optimized 2Dc-stack_elastic-lpw
2Dd: 2Dd-counter 2Dd-stack 2Dd-queue_optimized 2Dd-queue 2Dd-queue_elastic-lpw 2Dd-queue_elastic-law 2Dd-deque
multi_ran: multi-ct-faa_ran multi-ct_ran multi-st_ran multi-ct_ran2c multi-st_ran2c multi-qu_ran2c multi-qu_ran multi-st_ran4c multi-ct_ran4c multi-st_ran8c multi-ct_ran8c multi-qu_ran4c multi-qu_ran8c
external_queues: queue-lcrq queue-ms queue-wf queue-k-segment
external_stacks: stack-treiber stack-elimination stack-k-segment

clean:
	$(MAKE) -C src/queue-ms clean
	$(MAKE) -C src/queue-lcrq clean
	$(MAKE) -C src/queue-wf clean
	$(MAKE) -C src/queue-k-segment clean
	$(MAKE) -C src/2Dd-queue clean
	$(MAKE) -C src/2Dd-queue_optimized clean
	$(MAKE) -C src/2Dd-queue_elastic-lpw clean
	$(MAKE) -C src/2Dd-queue_elastic-law clean
	$(MAKE) -C src/multi-queue_random-relaxed clean
	$(MAKE) -C src/multi-queue_random-relaxed "CHOICES=two" clean
	$(MAKE) -C src/multi-queue_random-relaxed "CHOICES=four" clean
	$(MAKE) -C src/multi-queue_random-relaxed "CHOICES=eight" clean

	$(MAKE) -C src/2Dd-deque clean
	$(MAKE) -C src/2Dc-deque clean


	$(MAKE) -C src/stack-treiber clean
	$(MAKE) -C src/stack-elimination clean
	$(MAKE) -C src/stack-k-segment clean
	$(MAKE) -C src/multi-stack_random-relaxed clean
	$(MAKE) -C src/multi-stack_random-relaxed "CHOICES=two" clean
	$(MAKE) -C src/multi-stack_random-relaxed "CHOICES=four" clean
	$(MAKE) -C src/multi-stack_random-relaxed "CHOICES=eight" clean
	$(MAKE) -C src/2Dd-stack clean
	$(MAKE) -C src/2Dc-stack clean
	$(MAKE) -C src/2Dc-stack_optimized clean
	$(MAKE) -C src/2Dc-stack_elastic-lpw clean

	$(MAKE) -C src/multi-counter-faa_random-relaxed clean
	$(MAKE) -C src/multi-counter_random-relaxed clean
	$(MAKE) -C src/multi-counter_random-relaxed "CHOICES=two" clean
	$(MAKE) -C src/multi-counter_random-relaxed "CHOICES=four" clean
	$(MAKE) -C src/multi-counter_random-relaxed "CHOICES=eight" clean
	$(MAKE) -C src/2Dd-counter clean
	$(MAKE) -C src/2Dc-counter clean

	rm -rf build

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

