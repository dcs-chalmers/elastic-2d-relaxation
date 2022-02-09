# 2D Framework for Semantic Relaxation

The 2D framework derives relaxed concurrent data structures, with *k-out-of-order* relaxation. *Relaxation* means that the data structures are allowed to return items in slightly the wrong order. In k-out-of-order, a FIFO queue is allowed to dequeue any of the top *k* items during a dequeue, instead of just the head, which enables superior scalability over strict designs.

The core idea of the 2D framework is to split a data structure over several disjoint sub-structures. Then a *window* is superimposed over the sub-structures, which defines an area where operations are allowed at each sub-structure. The idea is that a thread can for the most part work alone on a sub-structure as long as it stays within the window. Then the window can be shifted when full or empty.

This repository is based on the [ASCYLIB framework](https://github.com/LPD-EPFL/ASCYLIB) and memory is managed using [SSMEM](https://github.com/LPD-EPFL/ssmem) which is a simple object-based memory allocator with epoch-based garbage collection.

## Related Publications
* [How to Relax Instantly: Elastic Relaxation of Concurrent Data Structures](https://arxiv.org/abs/2403.13644)
  * Kåre von Geijer, Philippas Tsigas.
  * To appear in proceedings of the 30th International European Conference on Parallel and Distributed Computing, Euro-Par 2024.
* [Monotonically Relaxing Concurrent Data-Structure Semantics for Increasing Performance: An Efficient 2D Design Framework](https://doi.org/10.4230/LIPIcs.DISC.2019.31)
  * Adones Rukundo, Aras Atalar, Philippas Tsigas.
  * In proceedings of the 33rd International Symposium on Distributed Computing, DISC 2019.
* [Brief Announcement: 2D-Stack - A Scalable Lock-Free Stack Design that Continuously Relaxes Semantics for Better Performance](https://doi.org/10.1145/3212734.3212794)
  * Adones Rukundo, Aras Atalar, Philippas Tsigas.
  * In proceedings of the 2018 ACM Symposium on Principles of Distributed Computing, PODC 2018.

## Designs

The `src` folder includes the data structures encompassed by the framework, as well as designs compared to in the publications. Each of these implementations has a _README.md_ file with additional information.

### Static 2D Designs

These designs are on a high level described in the [DISC paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31), and form the foundation of the 2D framework. They have had some optimizations done in conjunction with later publications.
- 2D queue: [./src/2Dd-queue](./src/2Dd-queue)
- Optimized 2D queue: [./src/2Dd-queue_optimized](./src/2Dd-queue_optimized)
- 2Dc stack: [./src/2Dc-stack](./src/2Dc-stack)
- Optimized 2Dc stack: [./src/2Dc-stack_optimized](./src/2Dc-stack_optimized)
- 2Dd stack: [./src/2Dd-stack](./src/2Dd-stack)
- 2Dd deque: [./src/2Dd-deque](./src/2Dd-deque)
- 2Dc counter: [./src/2Dc-counter](./src/2Dc-counter)
- 2Dd counter: [./src/2Dd-counter](./src/2Dd-counter)

### Elastic 2D Designs

These designs extend the 2D stack and queue to encompass _elastic relaxation_. This means that their degree of relaxation can be changed (either manually or with a dynamic controller) during runtime. They are described in the coming Euro-Par paper.
- 2D Lateral-as-Window (LaW) queue: [./src/2Dd-queue_elastic-law](./src/2Dd-queue_elastic-law)
- 2D Lateral-plus-Window (LpW) queue: [./src/2Dd-queue_elastic-lpw](./src/2Dd-queue_elastic-lpw)
- 2D Lateral-plus-Window (LpW) stack: [./src/2Dc-stack_elastic](./src/2Dc-stack_elastic)

### External Designs

These are implementations, or copies, of data structure designs used to compare the 2D designs against. These ar the main ones, used in the Euro-Par paper, but there are some additional ones as well in [./src/](./src/).
- Michael-Scott lock-free queue: [./src/queue-ms](./src/queue-ms/)
- Wait-free queue as fast as FAA: [./src/queue-wf](./src/queue-wf/)
- k-Segment queue: [./src/queue-k-segment](./src/queue-k-segment/)
- Treiber stack: [./src/stack-treiber](./src/stack-treiber/)
- Elimination stack: [./src/stack-elimination](./src/stack-elimination/)
- k-Segment stack: [./src/stack-k-segment](./src/stack-k-segment/)

## Usage

Simply clone the repository and run `make` to compile all implementations, using their default tests and switches. You can then find and run the respective data structure binary benchmark in `bin/`.

The default benchmark is a synthetic test where each thread repeatedly flips a coin to either insert or remove an item. The binary takes several cli flags, which are all described by running it with `-h`, such as `make 2Dc-queue && ./bin/2Dc-queue -h`. However, the most important arguments to get started might be:
- `-d`: The duration in ms to run the experiment,
- `-i`: The number of items to insert before starting the benchmark,
- `-n`: The number of threads to use,
- `-l`: The depth of the 2D window, only for the 2D data structures,
- `-w`: The width of the 2D window, mainly for the 2D and k-segment data structures.

### Prerequisites
The 2D code is designed to be run only on x86-64 machines, such as Intel or AMD. This is in part due to what memory ordering is assumed from the processor, and also due to the use of 128 bit compare and swaps. Even if runnable on other architectures, the relaxation bounds will likely not hold, due to additional possible reorderings.

Furthermore, you need `gcc` and `make` to compile the tests. To run the helper scripts in [scripts/](./scripts/), you need `bc` and `python 3`. Run the following command to install the required python packages `pip3 install numpy==1.26.3 matplotlib==3.8.2 scipy==1.12.0`.

### Recreating paper plots

To recreate the plots from the Euro-Par paper, run [scripts/recreate-europar.sh](./scripts/recreate-europar.sh), which will output its plots in the  ``results`` folder. Parameters such as number of threads can be adjusted at the top of the script.

### Compilation details
Either navigate a the data structure directory and run `make`, or run `make <data structure name>` from top level, which compiles the data structure tests with the default settings. You can further set different environment variables, such as `make VERSION=O3 GC=1 INIT=one` to modify the compilation. For all possible compilation switches, see [./common/Makefile.common](./common/Makefile.common) as well as the individual Makefile for each test. Here are the most common ones:
* `VERSION` defines the optimisation level e.g. `VERSION=O3`. It takes one of the following five values:
  * `DEBUG` compile with optimisation zero and debug flags
  * `SYMBOL` compile with optimisation level three and -g
  *  `O0` compile with optimisation level zero
  *  `O1` compile with optimisation level one
  *  `O2` compile with optimisation level two
  *  `O3` compile with optimisation level three (Default)
  *  `O4` compile with optimisation level three, and without any asserts
*  `GC` defines if deleted nodes should be recycled `GC=1` (Default) or not `GC=0`.
*  `INIT` defines if data structure initialization should be performed by all active threads `INIT=all` (Default) or one of the threads `INIT=one`
* `RELAXATION_ANALYSIS` can be set to `1` to measure relaxation levels for relaxed data structures. Keep in mind that this imposes a sequential order of linearizations, totally destroying the throughput at high thread counts.
* `TEST` can be used to change the benchmark used. This has mainly been used in the elastic data structures for testing dynamic scenarios, and possible switches can for example be seen in [src/2Dd-queue_elastic-law/Makefile](./src/2Dd-queue_elastic-law/Makefile).

### Directory description
* [src/](./src/): Contains the data structures' source code.
* [scripts/](./scripts/): Contains supporting scripts, such as ones aggregating several benchmark runs into plots.
* [results/](./results/): Default folder for test output of the scripts in [scripts](./scripts/).
* [include/](./include/): Contains support files e.g. the basic window framework ([2Dc-window.c](./include/2Dc-window.c)).
* [common/](./common/): Contains make definitions file.
* [external/](./external/): Contains external libraries such as the [ssmem](https://github.com/LPD-EPFL/ssmem) library.
* [bin/](./bin/): Contains the binary files for the compiled data structure benchmarks.

### Thread pinning
All tests will pin each software pthread to a hardware thread, using the function `pthread_setaffinity_np`. By default, the threads will be pinned sequentially to hardware threads `0,` 1, 2, 3...`. However, the numbering of the hardware threads depends on the specific CPU used, and you might often want to use a different order than the default one. For example, on a dual-socket Intel Xeon E5-2695 v4 system, the even hardware threads are on the first socket while the odd numbers are on the second, and you might not want to benchmark intra-socket behavior.

Here is a short step-by-step instruction for how to add a machine-specific pinning order:
- First, see e.g. the output from `lscpu` and `lstopo` (here we care about the `P#<...>` numbers) to understand the hardware topology.
- Then add an entry for your machine in [common/Makefile.common](./common/Makefile.common). For example, copy the one for the `athena` machine (start with `ifeq ($(PC_NAME), athena)...`), but change `athena` in the aforementioned line to the name of your computer (see output of `uname -n`), and change `ATHENA` in `-DATHENA` to a similar identifier for your computer (to be used in [include/utils.h](./include/utils.h)).
- Finally, add a matching entry to the aforementioned identifier in [include/utils.h](./include/utils.h). This entry primarily defines the order in which to pin the software threads to hardware threads. Here again, you can look at the entry for `ATHENA` for inspiration. There are three memory layouts, but the default one (the bottom-most one) is the most important to add, which should pin a thread to each core in a socket, before continuing with SMT, and finally proceeding to the next socket.
      - For a simpler example, see e.g. `ITHACA`.
Now all tests will use this pinning order. You can validate pinning orders by not allocating all hardware threads, and inspecting the output from `htop` during a test run.

