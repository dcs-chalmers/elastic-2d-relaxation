#!/bin/sh

# Variables and tests for running shorter than the real paper
nbr_threads=256             # Set to the number of threads you want to use
duration=500                # Reducing more will not have that big an effect, as the setup time is not included here
runs=1                      # When set to 1, only runs one run for each data point in scalability experiments
step=$((nbr_threads / 4))   # Decrease this to get a more detailed plot
rel_start=100
variable_runs=10            # Can be reduced to lessen the run-time of the last two plots

# Variables and used in the paper
# nbr_threads=256
# duration=1000
# runs=10
# step=$((nbr_threads / 8))
# rel_start=28
# variable_runs=50

# Thread scalability
python3 scripts/benchmark.py --errors --initial 524288 -k 5000 -m 1 --runs $runs --width-ratio 2 --start 1 --to $nbr_threads --step $step --include_start -d $duration --ndebug 2Dd-queue_elastic-lpw 2Dd-queue_elastic-law 2Dd-queue_optimized queue-k-segment queue-wf queue-ms --title "Queues: Thread Scalability" --name queues_k5000
python3 scripts/benchmark.py --errors --initial 524288 -k 5000 -m 1 --runs $runs --width-ratio 2 --start 1 --to $nbr_threads --step $step --include_start -d $duration --ndebug 2Dc-stack_elastic-lpw 2Dc-stack_optimized stack-k-segment stack-elimination stack-treiber         --title "Stacks: Thread Scalability" --name stacks_k5000

# Relaxation scaliability
python3 scripts/benchmark.py --errors --initial 524288 -n $nbr_threads -f $rel_start -t 102400 -v k --exp_steps --runs $runs --width-ratio 2 -d $duration -m 1 --ndebug 2Dd-queue_elastic-lpw 2Dd-queue_elastic-law 2Dd-queue_optimized queue-k-segment queue-wf queue-ms --title "Queues: Relaxation Scalability" --name queues-relax_n$nbr_threads
python3 scripts/benchmark.py --errors --initial 524288 -n $nbr_threads -f $rel_start -t 102400 -v k --exp_steps --runs $runs --width-ratio 2 -d $duration -m 1 --ndebug 2Dc-stack_elastic-lpw 2Dc-stack_optimized stack-k-segment stack-elimination stack-treiber         --title "Stacks: Relaxation Scalability" --name stacks-relax_n$nbr_threads


# Variable workload procucer-consumer
# The number of initial items (-i flag) can be adapted to avoid too many empty returns, while keeping the queue relatively small
python3 scripts/benchmark-variable-workload.py 2Dd-queue_elastic-law --args "-i 83886080" "-l 16" "-d 1000" "-w $nbr_threads" "-n $nbr_threads" --show --title "LaW Queue: Variable Workload" --ops-per-ts 500  --runs $variable_runs --test variable-workload-static --name variable-law-queue-short-static
python3 scripts/benchmark-variable-workload.py 2Dd-queue_elastic-law --args "-i 8388608"  "-l 16" "-d 1000" "-w $nbr_threads" "-n $nbr_threads" --show --title "LaW Queue: Variable Workload" --ops-per-ts 500  --runs $variable_runs                                 --name variable-law-queue-short
