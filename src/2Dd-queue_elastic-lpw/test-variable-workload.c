/*
	*   File: variable-workload.c
	*   Author: Kåre von Geijer
	*			Adones Rukundo
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
*/

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <malloc.h>
#include "utils.h"
#include "atomic_ops.h"
#include "rapl_read.h"
#ifdef __sparc__
	#include <sys/types.h>
	#include <sys/processor.h>
	#include <sys/procset.h>
#endif

#include "2Dd-queue_elastic.h"

#ifdef RELAXATION_ANALYSIS
	#include "relaxation_analysis_queue.h"
#endif

#if !defined(VALIDATESIZE)
	#define VALIDATESIZE 1
#endif

/* ################################################################### *
	* GLOBALS
* ################################################################### */

RETRY_STATS_VARS_GLOBAL;

size_t initial = DEFAULT_INITIAL;
size_t range = DEFAULT_RANGE;
size_t update = 100;
size_t load_factor;
size_t num_threads = DEFAULT_NB_THREADS;
size_t duration = DEFAULT_DURATION;

size_t print_vals_num = 100;
size_t pf_vals_num = 1023;
size_t put, put_explicit = false;
double update_rate, put_rate, get_rate;

size_t size_after = 0;
int seed = 0;
uint32_t rand_max;
#define rand_min 1

static volatile int stop;
uint64_t relaxation_bound = 1;
uint64_t width = 1;
uint64_t depth = 1;
uint8_t k_mode = 0;
size_t side_work = 0;

TEST_VARS_GLOBAL;

volatile ticks *putting_succ;
volatile ticks *putting_fail;
volatile ticks *removing_succ;
volatile ticks *removing_fail;
volatile ticks *putting_count;
volatile ticks *putting_count_succ;
volatile unsigned long *put_cas_fail_count;
volatile unsigned long *get_cas_fail_count;
volatile unsigned long *null_count;
volatile unsigned long *hop_count;
volatile unsigned long *slide_count;
volatile unsigned long *slide_fail_count;
volatile ticks *removing_count;
volatile ticks *removing_count_succ;
volatile ticks *total;

// Added for throughput over time
volatile uint64_t **timestamps;
volatile width_t **widths;
volatile uint64_t **error_dists_sizes;
volatile long start_sec;


/* ################################################################### *
	* LOCALS
* ################################################################### */

#ifdef DEBUG
	extern __thread uint32_t put_num_restarts;
	extern __thread uint32_t put_num_failed_expand;
	extern __thread uint32_t put_num_failed_on_new;
#endif

__thread unsigned long *seeds;
__thread ssmem_allocator_t* alloc;
__thread unsigned long my_put_cas_fail_count;
__thread unsigned long my_get_cas_fail_count;
__thread unsigned long my_null_count;
__thread unsigned long my_hop_count;
__thread unsigned long my_slide_count;
__thread unsigned long my_slide_fail_count;
__thread int thread_id;

barrier_t barrier, barrier_global;

typedef struct thread_data
{
	uint32_t id;
	DS_TYPE* set;
} thread_data_t;

// Convert a time interval in fractional seconds to nanoseconds
uint64_t interval_to_nanoseconds(double interval) {
    return (uint64_t)(interval * 1000000000);
}
// Convert seconds and nanoseconds to a single 64-bit integer representing nanoseconds
uint64_t to_nanoseconds(int sec, long nsec) {
    return (uint64_t)sec * 1000000000 + nsec;
}

// Get current time in nanoseconds since an unspecified starting point (e.g., system boot)
uint64_t get_current_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return to_nanoseconds(ts.tv_sec, ts.tv_nsec);
}

// Sleep until the specified target time in nanoseconds
void sleep_until(uint64_t target_time_ns) {
    uint64_t now_ns = get_current_time_ns();
    while (now_ns < target_time_ns) {
        struct timespec ts;
        uint64_t sleep_time_ns = target_time_ns - now_ns;
        ts.tv_sec = sleep_time_ns / 1000000000;
        ts.tv_nsec = sleep_time_ns % 1000000000;
        nanosleep(&ts, NULL);
        now_ns = get_current_time_ns();
    }
}

void* test_loop_producer(DS_TYPE* set, uint64_t* my_putting_count_succ, uint64_t* my_putting_count)
{
	// Directly copied from main_test_loop, TEST_ONLY_UPDATES, for the producer-consumer scenario
	uint32_t c = my_random(&(seeds[0]),&(seeds[1]),&(seeds[2]));

	uint32_t key = (c & rand_max) + rand_min;
	START_TS(1);
	int res = DS_ADD(set, key, key);

	// END_TS(1, my_putting_count_succ);
	ADD_DUR(my_putting_succ);
	*my_putting_count_succ += 1;

	*my_putting_count += 1;

	// cpause(side_work);

}

void* test_loop_consumer(DS_TYPE* set, uint64_t* my_removing_count, uint64_t* my_removing_count_succ)
{
	// Directly copied from main_test_loop, TEST_ONLY_UPDATES, for the producer-consumer scenario
	uint32_t c = my_random(&(seeds[0]),&(seeds[1]),&(seeds[2]));

	int removed;
	// START_TS(2);
	removed = DS_REMOVE(set);

	if(removed != 0)
	{
		// END_TS(2, my_removing_count_succ);
		// ADD_DUR(my_removing_succ);
		*my_removing_count_succ += 1;
	}

	// END_TS_ELSE(5, my_removing_count - *my_removing_count_succ, my_removing_fail);
	*my_removing_count += 1;

	// cpause(side_work);

}

int is_producer(int id)
{
	return id < (num_threads*2)/3;
	// return id < (3*num_threads)/4;
}

double* get_time_intervals(int thread_id, int* nbr_intervals)
{
	int prod = 0;
	for (int i = 0; i < num_threads; i++) if (is_producer(i)) prod++;
	int prod_before = 0;
	for (int i = 0; i < thread_id; i++) if (is_producer(i)) prod_before++;

	// 0.0 to 1.0 for the producers
	double thread_percentage = (double) prod_before / (double) (prod - 1);

	double* intervals = calloc(sizeof(double), 32);

	double time_unit = (double) duration / 1000.0 / 10.0; // OBS: update as more units are added

	int interval_ind = 0;
	double time_at = 0.0;

	#define WORK_PERIOD(time_from, time_to) if (interval_ind>0 && (time_from - intervals[interval_ind-1]) < 0.000001) { intervals[interval_ind-1] = time_to; } else { intervals[interval_ind++] = time_from; intervals[interval_ind++] = time_to; }

	// Let't just work all the time
	// WORK_PERIOD(time_at, time_at+10*time_unit);
	// *nbr_intervals = interval_ind;
	// return intervals;

	// 50% work one unit at the start
	if (thread_percentage < 0.5) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	// Another 25% join over one unit, and then they all work one unit
	// Another 50% join over two units
	if (thread_percentage < 0.5) {
		WORK_PERIOD(time_at, time_at + 2*time_unit);
	} else if (thread_percentage < 0.75) {
		double time_offset = (thread_percentage-0.5)/0.25*1.5*time_unit;
		WORK_PERIOD(time_at + time_offset, time_at + 2*time_unit);
	}
	time_at += 2*time_unit;

	// 25% continue to work for two units, the rest stop gradually over one
	if (thread_percentage < 0.5) {
		double time_offset = thread_percentage/0.55*time_unit;
		WORK_PERIOD(time_at, time_at + time_offset);
	} else if (thread_percentage < 0.75) {
		WORK_PERIOD(time_at, time_at + 2*time_unit);
	}
	time_at += 2*time_unit;

	// all work for one unit
	if (1) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	// 40% work for one unit
	if (thread_percentage < 0.40) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	// 10% work for one unit
	if (thread_percentage < 0.10) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	// 80% work for one unit
	if (thread_percentage < 0.8) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	// 50% work for one unit
	if (thread_percentage < 0.5) {
		WORK_PERIOD(time_at, time_at + time_unit);
	}
	time_at += time_unit;

	*nbr_intervals = interval_ind;
	return intervals;
}

void* test(void* thread)
{
	thread_data_t* td = (thread_data_t*) thread;
	thread_id = td->id;
	set_cpu(thread_id);
	ssalloc_init();

	DS_TYPE* set = td->set;

	int nbr_intervals;
	double* time_intervals = get_time_intervals(thread_id, &nbr_intervals);

	THREAD_INIT(thread_id);
	PF_INIT(3, SSPFD_NUM_ENTRIES, thread_id);

	#if defined(COMPUTE_LATENCY)
		volatile ticks my_putting_succ = 0;
		volatile ticks my_putting_fail = 0;
		volatile ticks my_removing_succ = 0;
		volatile ticks my_removing_fail = 0;
	#endif
	uint64_t my_putting_count = 0;
	uint64_t my_removing_count = 0;

	uint64_t my_putting_count_succ = 0;
	uint64_t my_removing_count_succ = 0;

	#if defined(COMPUTE_LATENCY) && PFD_TYPE == 0
		volatile ticks start_acq, end_acq;
		volatile ticks correction = getticks_correction_calc();
	#endif

	seeds = seed_rand();
	#if GC == 1
		alloc = (ssmem_allocator_t*) malloc(sizeof(ssmem_allocator_t));
		assert(alloc != NULL);
		ssmem_alloc_init_fs_size(alloc, SSMEM_DEFAULT_MEM_SIZE, SSMEM_GC_FREE_SET_SIZE, thread_id);
	#endif

	RR_INIT(thread_id);
	barrier_cross(&barrier);

	uint64_t key;
	int c = 0;
	uint32_t scale_rem = (uint32_t) (update_rate * UINT_MAX);
	uint32_t scale_put = (uint32_t) (put_rate * UINT_MAX);

	int i;
	uint32_t num_elems_thread = (uint32_t) (initial / num_threads);
	int32_t missing = (uint32_t) initial - (num_elems_thread * num_threads);
	if (thread_id < missing)
    {
		num_elems_thread++;
	}

	#if INITIALIZE_FROM_ONE == 1
		num_elems_thread = (thread_id == 0) * initial;
	#endif
	for(i = 0; i < num_elems_thread; i++)
    {
		key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (rand_max + 1)) + rand_min;

		if(enqueue(set, key, key, false) == false)
		{
			i--;
		}
	}

	MEM_BARRIER;
	barrier_cross(&barrier);
	if (!thread_id)
    {
		printf("BEFORE size is, %zu\n", (size_t) DS_SIZE(set));
	}

	RETRY_STATS_ZERO();

	// Set up timing structures
	#define MAX_TIMESTAMPS 3.2e6

 	uint64_t *my_timestamps = calloc(MAX_TIMESTAMPS, sizeof(uint64_t));
 	width_t *my_widths = calloc(MAX_TIMESTAMPS, sizeof(width_t));
	size_t timeit = 0;
	#ifdef RELAXATION_ANALYSIS
		// Tracks which is the most recent node at each timestamp
		uint64_t* my_error_dists_sizes = calloc(MAX_TIMESTAMPS, sizeof(void*));
	#endif


	barrier_cross(&barrier_global);
	uint64_t t0 = get_current_time_ns();
	int interval_ind = 0;

	RR_START_SIMPLE();

	if (is_producer(thread_id))
	{
		while (interval_ind < nbr_intervals)
	    {
			// TOOD: Dont try to sleep or time if we work in this and next (check if sleep is 0)

			// Sleep until the relative timestamp
			sleep_until(t0 + interval_to_nanoseconds(time_intervals[interval_ind++]));

			// Then work until the next timestamp
			uint64_t target_time_ns = t0 + interval_to_nanoseconds(time_intervals[interval_ind++]);
	        while (get_current_time_ns() < target_time_ns)
			{
				for (int i = 0; i < OPS_PER_TS; i++)
				{
					test_loop_producer(set, &my_putting_count, &my_putting_count_succ);
					// TEST_LOOP_ONLY_UPDATES();
				}
				#ifdef RELAXATION_ANALYSIS
					lock_relaxation_lists();
				#endif

			    // Calculate the elapsed time in nanoseconds
				my_widths[timeit] = get_put_width();
				my_timestamps[timeit] = get_current_time_ns();

				#ifdef RELAXATION_ANALYSIS
					my_error_dists_sizes[timeit] = error_dists.size;
					unlock_relaxation_lists();
				#endif
				timeit += 1;
	        }
			my_widths[timeit] = get_put_width();
		    // Set a 1 to signify that you have gone to sleep (1 is reserved for this..)
			my_timestamps[timeit++] = 1;
		}
	}
	else
	{
		while (!stop)
		{
			for (int i = 0; i < OPS_PER_TS; i++)
			{
				test_loop_consumer(set, &my_removing_count, &my_removing_count_succ);
				// TEST_LOOP_ONLY_UPDATES();
			}
			#ifdef RELAXATION_ANALYSIS
				lock_relaxation_lists();
			#endif

		    // Calculate the elapsed time in nanoseconds
			my_timestamps[timeit] = get_current_time_ns();
			my_widths[timeit] = get_get_width();

			#ifdef RELAXATION_ANALYSIS
				my_error_dists_sizes[timeit] = error_dists.size;
				unlock_relaxation_lists();
			#endif
			timeit += 1;
		}
	}


	barrier_cross(&barrier);
	RR_STOP_SIMPLE();
	if (!thread_id)
    {
		size_after = DS_SIZE(set);
		printf("AFTER size is, %zu \n", size_after);
	}

	barrier_cross(&barrier);

	// Share the over time timestamps
	timestamps[thread_id] = my_timestamps;
	widths[thread_id] = my_widths;
	#ifdef RELAXATION_ANALYSIS
		error_dists_sizes[thread_id] = my_error_dists_sizes;
	#endif
	if (timeit > MAX_TIMESTAMPS) {
		exit(-1);
	}

	#if defined(COMPUTE_LATENCY)
		putting_succ[thread_id] += my_putting_succ;
		putting_fail[thread_id] += my_putting_fail;
		removing_succ[thread_id] += my_removing_succ;
		removing_fail[thread_id] += my_removing_fail;
	#endif
	putting_count[thread_id] += my_putting_count;
	removing_count[thread_id]+= my_removing_count;

	putting_count_succ[thread_id] += my_putting_count_succ;
	removing_count_succ[thread_id]+= my_removing_count_succ;

	put_cas_fail_count[thread_id]=my_put_cas_fail_count;
	get_cas_fail_count[thread_id]=my_get_cas_fail_count;
	null_count[thread_id]=my_null_count;
	hop_count[thread_id]=my_hop_count;
	slide_count[thread_id]=my_slide_count;
	slide_fail_count[thread_id]=my_slide_fail_count;

	EXEC_IN_DEC_ID_ORDER(thread_id, num_threads)
    {
		print_latency_stats(thread_id, SSPFD_NUM_ENTRIES, print_vals_num);
		RETRY_STATS_SHARE();
	}
	EXEC_IN_DEC_ID_ORDER_END(&barrier);

	SSPFDTERM();
	#if GC == 1
		ssmem_term();
		free(alloc);
	#endif
	THREAD_END();
	pthread_exit(NULL);
}

// creates a timespec from a duration ms
struct timespec calc_timeout(long duration) {
	struct timespec timeout;
	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;
	return timeout;
}

// To print at what timestamps we wanted to change the relaxation
void print_timestamp(width_t new_width, depth_t new_depth) {
	volatile struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double time = ((double) now.tv_nsec) + ((double) now.tv_sec) * 1e9;
	printf("Initiating change to width %d and depth %d at timestamp %.0f\n", new_width, new_depth, time);
}

// To use for reporting relaxation errors in a nice sorted way
#ifdef RELAXATION_ANALYSIS
	typedef struct {
	    double timestamp;
	    size_t size;
	} TimeSizeTuple;

	// Comparison function for qsort
	int compare_timetuple(const void *a, const void *b) {
    TimeSizeTuple *tupleA = (TimeSizeTuple *)a;
    TimeSizeTuple *tupleB = (TimeSizeTuple *)b;
		double diff = tupleA->timestamp - tupleB->timestamp;
    if (diff > 0.000001) return 1;
    if (diff < -0.000001) return -1;
    return 0;
	}
#endif

int main(int argc, char **argv)
{
	set_cpu(0);
	ssalloc_init();
	seeds = seed_rand();

	struct option long_options[] = {
		// These options don't set a flag
		{"help",                      no_argument,       NULL, 'h'},
		{"duration",                  required_argument, NULL, 'd'},
		{"initial-size",              required_argument, NULL, 'i'},
		{"num-threads",               required_argument, NULL, 'n'},
		{"range",                     required_argument, NULL, 'r'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"num-buckets",               required_argument, NULL, 'b'},
		{"print-vals",                required_argument, NULL, 'v'},
		{"vals-pf",                   required_argument, NULL, 'f'},
		{NULL, 0, NULL, 0}
	};

	int i, c;
	while(1)
    {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:n:r:u:m:a:l:p:b:v:f:y:z:k:w:s:", long_options, &i);
		if(c == -1)
		break;
		if(c == 0 && long_options[i].flag == 0)
		c = long_options[i].val;
		switch(c)
		{
			case 0:
			/* Flag is automatically set */
			break;
			case 'h':
			printf("ASCYLIB -- stress test "
			"\n"
			"\n"
			"Usage:\n"
			"  %s [options...]\n"
			"\n"
			"Options:\n"
			"  -h, --help\n"
			"        Print this message\n"
			"  -d, --duration <int>\n"
			"        Test duration in milliseconds\n"
			"  -i, --initial-size <int>\n"
			"        Number of elements to insert before test\n"
			"  -n, --num-threads <int>\n"
			"        Number of threads\n"
			"  -r, --range <int>\n"
			"        Range of integer values inserted in set\n"
			"  -u, --update-rate <int>\n"
			"        Percentage of update transactions\n"
			"  -p, --put-rate <int>\n"
			"        Percentage of put update transactions (should be less than percentage of updates)\n"
			"  -b, --num-buckets <int>\n"
			"        Number of initial buckets (stronger than -l)\n"
			"  -v, --print-vals <int>\n"
			"        When using detailed profiling, how many values to print.\n"
			"  -f, --val-pf <int>\n"
			"        When using detailed profiling, how many values to keep track of.\n"
			"  -s, --side-work <int>\n"
			"        thread work between data structure access operations.\n"
			"  -k, --Relaxation-bound <int>\n"
			"        Relaxation bound.\n"
			"  -l, --Depth <int>\n"
			"        Locality/Depth if k-mode is set to zero.\n"
			"  -w, --Width <int>\n"
			"        Fixed Width or Width to thread ratio depending on the k-mode.\n"
			"  -m, --K Mode <int>\n"
			"        0 for Fixed Width and Depth, 1 for Fixed Width, 2 for fixed Depth, 3 for fixed Width to thread ratio.\n"
			, argv[0]);
			exit(0);
			case 'd':
			duration = atoi(optarg);
			break;
			case 'i':
			initial = atoi(optarg);
			break;
			case 'n':
			num_threads = atoi(optarg);
			break;
			case 'r':
			range = atol(optarg);
			break;
			case 'u':
			update = atoi(optarg);
			break;
			case 'p':
			put_explicit = 1;
			put = atoi(optarg);
			break;
			case 'v':
			print_vals_num = atoi(optarg);
			break;
			case 'f':
			pf_vals_num = pow2roundup(atoi(optarg)) - 1;
			break;
			case 's':
			side_work = atoi(optarg);
			break;
			case 'k':
			if(atoi(optarg)>0) relaxation_bound = atoi(optarg);
			break;
			case 'l':
			if(atoi(optarg)>0) depth = atoi(optarg);
			break;
			case 'w':
			if(atoi(optarg)>0) width = atoi(optarg);
			break;
			case 'm':
			if(atoi(optarg)<=3) k_mode = atoi(optarg);
			break;
			break;
			case '?':
			default:
			printf("Use -h or --help for help\n");
			exit(1);
		}
	}


	if (!is_power_of_two(initial))
	{
		size_t initial_pow2 = pow2roundup(initial);
		printf("** rounding up initial (to make it power of 2): old: %zu / new: %zu\n", initial, initial_pow2);
		initial = initial_pow2;
	}

	if (range < initial)
	{
		range = 2 * initial;
	}

	printf("Initial, %zu \n", initial);
	printf("Range, %zu \n", range);
	printf("Algorithm, OPTIK \n");

	double kb = initial * sizeof(DS_NODE) / 1024.0;
	double mb = kb / 1024.0;
	printf("Sizeof initial, %.2f KB is %.2f MB\n", kb, mb);

	if (!is_power_of_two(range))
	{
		size_t range_pow2 = pow2roundup(range);
		printf("** rounding up range (to make it power of 2): old: %zu / new: %zu\n", range, range_pow2);
		range = range_pow2;
	}

	if (put > update)
	{
		put = update;
	}

	update_rate = update / 100.0;

	if (put_explicit)
	{
		put_rate = put / 100.0;
	}
	else
	{
		put_rate = update_rate / 2;
	}
	get_rate = 1 - update_rate;

	rand_max = range - 1;

	struct timeval start, end;
	stop = 0;

	DS_TYPE* set = DS_NEW(num_threads, width, depth, 65535, k_mode, relaxation_bound);
	assert(set != NULL);

	/* Initializes the local data */
	putting_succ = (ticks *) calloc(num_threads , sizeof(ticks));
	putting_fail = (ticks *) calloc(num_threads , sizeof(ticks));
	removing_succ = (ticks *) calloc(num_threads , sizeof(ticks));
	removing_fail = (ticks *) calloc(num_threads , sizeof(ticks));
	putting_count = (ticks *) calloc(num_threads , sizeof(ticks));
	putting_count_succ = (ticks *) calloc(num_threads , sizeof(ticks));
	removing_count = (ticks *) calloc(num_threads , sizeof(ticks));
	removing_count_succ = (ticks *) calloc(num_threads , sizeof(ticks));
	put_cas_fail_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	get_cas_fail_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	null_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	slide_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	slide_fail_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	hop_count = (unsigned long *) calloc(num_threads , sizeof(unsigned long));
	timestamps = calloc(num_threads, sizeof(double*));
	widths = calloc(num_threads, sizeof(double*));
	error_dists_sizes = calloc(num_threads, sizeof(uint64_t*));

	pthread_t threads[num_threads];
	pthread_attr_t attr;
	int rc;
	void *status;

	#ifdef RELAXATION_ANALYSIS
		// Initialize relaxation analysis
		init_relaxation_analysis();
	#endif

	//ad initialize barriers
	barrier_init(&barrier_global, num_threads + 1);
	barrier_init(&barrier, num_threads);

	/* Initialize and set thread detached attribute */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	thread_data_t* tds = (thread_data_t*) malloc(num_threads * sizeof(thread_data_t));

	long t;
	for(t = 0; t < num_threads; t++)
	{
		tds[t].id = t;
		tds[t].set = set;
		rc = pthread_create(&threads[t], &attr, test, tds + t); //ad create thread and call test function
		if (rc)
		{
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}


	/* Free attribute and wait for the other threads */
	pthread_attr_destroy(&attr);
	/*main thread will wait on the &barrier_global until all threads within test have reached
	and set the timer before they cross to start the test loop*/
	barrier_cross(&barrier_global);
	uint64_t t0 = get_current_time_ns();

	sleep_until(t0 + duration*1e6);

	// Done!
	stop = 1;
	uint64_t t_end = get_current_time_ns();
	duration = (t_end - t0)/1000000.0;

	for(t = 0; t < num_threads; t++)
	{
		rc = pthread_join(threads[t], &status);
		if (rc)
		{
			printf("ERROR; return code from pthread_join() is %d\n", rc);
			exit(-1);
		}
	}

	free(tds);

	volatile ticks putting_suc_total = 0;
	volatile ticks putting_fal_total = 0;
	volatile ticks removing_suc_total = 0;
	volatile ticks removing_fal_total = 0;
	volatile uint64_t putting_count_total = 0;
	volatile uint64_t putting_count_total_succ = 0;
	volatile unsigned long put_cas_fail_count_total = 0;
	volatile unsigned long get_cas_fail_count_total = 0;
	volatile unsigned long null_count_total = 0;
	volatile unsigned long slide_count_total = 0;
	volatile unsigned long slide_fail_count_total = 0;
	volatile unsigned long hop_count_total = 0;
	volatile uint64_t removing_count_total = 0;
	volatile uint64_t removing_count_total_succ = 0;

	for(t=0; t < num_threads; t++)
	{
		PRINT_OPS_PER_THREAD();
		putting_suc_total += putting_succ[t];
		putting_fal_total += putting_fail[t];
		removing_suc_total += removing_succ[t];
		removing_fal_total += removing_fail[t];
		putting_count_total += putting_count[t];
		putting_count_total_succ += putting_count_succ[t];
		put_cas_fail_count_total += put_cas_fail_count[t];
		get_cas_fail_count_total += get_cas_fail_count[t];
		null_count_total += null_count[t];
		hop_count_total += hop_count[t];
		slide_count_total += slide_count[t];
		slide_fail_count_total += slide_fail_count[t];
		removing_count_total += removing_count[t];
		removing_count_total_succ += removing_count_succ[t];
	}

	#if defined(COMPUTE_LATENCY)
		printf("#thread srch_suc srch_fal insr_suc insr_fal remv_suc remv_fal   ## latency (in cycles) \n"); fflush(stdout);
		long unsigned put_suc = putting_count_total_succ ? putting_suc_total / putting_count_total_succ : 0;
		long unsigned put_fal = (putting_count_total - putting_count_total_succ) ? putting_fal_total / (putting_count_total - putting_count_total_succ) : 0;
		long unsigned rem_suc = removing_count_total_succ ? removing_suc_total / removing_count_total_succ : 0;
		long unsigned rem_fal = (removing_count_total - removing_count_total_succ) ? removing_fal_total / (removing_count_total - removing_count_total_succ) : 0;
		printf("%-7zu %-8lu %-8lu %-8lu %-8lu %-8lu %-8lu\n", num_threads, get_suc, get_fal, put_suc, put_fal, rem_suc, rem_fal);
	#endif

	#define LLU long long unsigned int

	int UNUSED pr = (int) (putting_count_total_succ - removing_count_total_succ);
	#if VALIDATESIZE==1
		if (size_after != (initial + pr))
		{
			printf("\n******** ERROR WRONG size. %zu + %d != %zu (difference %zu)**********\n\n", initial, pr, size_after, (initial + pr)-size_after);
			// assert(size_after == (initial + pr));
		}
	#endif
	uint64_t total = putting_count_total + removing_count_total;
	double putting_perc = 100.0 * (1 - ((double)(total - putting_count_total) / total));
	double putting_perc_succ = (1 - (double) (putting_count_total - putting_count_total_succ) / putting_count_total) * 100;
	double removing_perc = 100.0 * (1 - ((double)(total - removing_count_total) / total));
	double removing_perc_succ = (1 - (double) (removing_count_total - removing_count_total_succ) / removing_count_total) * 100;

	printf("putting_count_total , %-10llu \n", (LLU) putting_count_total);
	printf("putting_count_total_succ , %-10llu \n", (LLU) putting_count_total_succ);
	printf("putting_perc_succ , %10.1f \n", putting_perc_succ);
	printf("putting_perc , %10.1f \n", putting_perc);
	printf("putting_effective , %10.1f \n", (putting_perc * putting_perc_succ) / 100);

	printf("removing_count_total , %-10llu \n", (LLU) removing_count_total);
	printf("removing_count_total_succ , %-10llu \n", (LLU) removing_count_total_succ);
	printf("removing_perc_succ , %10.1f \n", removing_perc_succ);
	printf("removing_perc , %10.1f \n", removing_perc);
	printf("removing_effective , %10.1f \n", (removing_perc * removing_perc_succ) / 100);

	double throughput = ((double) (putting_count_total + removing_count_total) * 1000.0) / (double) duration;

	printf("num_threads , %zu \n", num_threads);
	printf("Mops , %.3f\n", throughput / 1e6);
	printf("Ops , %.2f\n", throughput);

	RR_PRINT_CORRECTED();
	RETRY_STATS_PRINT(total, putting_count_total, removing_count_total, putting_count_total_succ + removing_count_total_succ);
	LATENCY_DISTRIBUTION_PRINT();

	printf("Push_CAS_fails , %zu\n", put_cas_fail_count_total);
	printf("Pop_CAS_fails , %zu\n", get_cas_fail_count_total);
	printf("Null_Count , %zu\n", null_count_total);
	printf("Hop_Count , %zu\n", hop_count_total);
	printf("Slide_Count , %zu\n", slide_count_total);
	printf("Slide-Fail_Count , %zu\n", slide_fail_count_total);
	printf("Width , %zu\n", set->width);
	printf("Depth , %zu\n", set->depth);
	printf("Relaxation_bound, %zu\n", set->relaxation_bound);
	printf("K_mode , %zu\n", set->k_mode);

	// Print throughput over time stats for each thread
	// First print how many updates per timestamp
	printf("\nUpdates per timestamp: %zu\n", ((long) OPS_PER_TS));

	int prod = 0;
	for (int i = 0; i < 1e6; i++) if (is_producer(i)) prod++;
	printf("Nbr producers: %d\n", prod);

	#ifndef RELAXATION_ANALYSIS
	for (int thread = 0; thread < num_threads; thread++) {
		for (int i = 0; timestamps[thread][i] != 0; i += 1) {
				if (is_producer(thread))
					printf("[%zu] prod timestamp %d: %zu ns, %zu width\n", thread, i, timestamps[thread][i], widths[thread][i]);
				else
					printf("[%zu] cons timestamp %d: %zu ns, %zu width\n", thread, i, timestamps[thread][i], widths[thread][i]);
		}
	}
	#else
	// Zip the timestamps and size values
	TimeSizeTuple *tuples = calloc(num_threads*MAX_TIMESTAMPS, sizeof(TimeSizeTuple));
	size_t zip_ind = 0;
	for (int thread = 0; thread < num_threads; thread++) {
		for (int i = 0; timestamps[thread][i] != 0; i += 1) {
      tuples[zip_ind].timestamp = timestamps[thread][i];
      tuples[zip_ind].size = error_dists_sizes[thread][i];
			zip_ind += 1;
		}
	}

  qsort(tuples, zip_ind, sizeof(TimeSizeTuple), compare_timetuple);

	// Now print the values, but only when there is an increase in size
	size_t error_pos = 0;
	linear_node_t* error_node = error_dists.head;

	for (int i = 0; i < zip_ind; i++) {
			// Skip these ones
			if (tuples[i].size <= error_pos) continue;

			uint64_t relaxed_sum = 0;
			uint64_t samples = tuples[i].size - error_pos;
			for (int _sample = 0; _sample < samples; _sample += 1)
			{
				relaxed_sum += error_node->val;
				error_node = error_node->next;
				error_pos += 1;
			}

			printf("[AGG] timestamp %d: %.0f ns, %.2f average period err over %zu samples\n", i, tuples[i].timestamp, ((double) relaxed_sum)/((double) samples), samples);
	}
	#endif

	#if defined(RELAXATION_ANALYSIS)
		print_relaxation_measurements();
	#endif

	pthread_exit(NULL);

	return 0;
}
