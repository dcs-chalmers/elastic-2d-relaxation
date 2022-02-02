.PHONY:	all

BENCHS =  src/deque-maged_2Dc-win src/deque-maged_2Dd-win src/multi-counter_2dd-window src/multi-stack_2dd-window src/multi-stack_random-relaxed src/multi-queue_random-relaxed src/multi-counter-faa_random-relaxed src/multi-counter_random-relaxed  src/multi-queue_2dd-window

.PHONY:	clean all external $(BENCHS)
	
default: 
	$(MAKE)  alg

multi-qu_2dd:
	$(MAKE) src/multi-queue_2dd-window
multi-qu_ran:
	$(MAKE) src/multi-queue_random-relaxed
multi-qu_ran2c:
	$(MAKE) "CHOICES=two" src/multi-queue_random-relaxed
multi-qu_ran4c:
	$(MAKE) "CHOICES=four" src/multi-queue_random-relaxed
multi-qu_ran8c:
	$(MAKE) "CHOICES=eight" src/multi-queue_random-relaxed
	

deque-maged_2Dd-win:
	$(MAKE) src/deque-maged_2Dd-win
deque-maged_2Dc-win:
	$(MAKE) src/deque-maged_2Dc-win

lfst_treiber:
	$(MAKE) src/stack-treiber
multi-st_ran:
	$(MAKE) src/multi-stack_random-relaxed
multi-st_ran2c:
	$(MAKE) "CHOICES=two" src/multi-stack_random-relaxed
multi-st_ran4c:
	$(MAKE) "CHOICES=four" src/multi-stack_random-relaxed
multi-st_ran8c:
	$(MAKE) "CHOICES=eight" src/multi-stack_random-relaxed
multi-st_2dd:
	$(MAKE) src/multi-stack_2dd-window
	
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
multi-ct_2dd:
	$(MAKE) src/multi-counter_2dd-window


multi_2dc: deque-maged_2Dc-win
multi_2dd: multi-ct_2dd multi-st_2dd multi-qu_2dd deque-maged_2Dd-win
multi_ran: multi-ct-faa_ran multi-ct_ran multi-st_ran multi-ct_ran2c multi-st_ran2c multi-qu_ran2c multi-qu_ran multi-st_ran4c multi-ct_ran4c multi-st_ran8c multi-ct_ran8c multi-qu_ran4c multi-qu_ran8c

alg: multi_2dd multi_ran multi_2dc

clean:
	$(MAKE) -C src/multi-queue_2dd-window clean
	rm -rf build

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

