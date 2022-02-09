#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include "common.h"
#include <atomic_ops.h>
#include "lock_if.h"
#include "ssmem.h"
#include "utils.h"

 /* ################################################################### *
	* Definition of macros: per data structure
* ################################################################### */

#define DS_CONTAINS(s,k,t)  set_contains(s)
#define DS_ADD(s,k,v)       increment(s)
#define DS_REMOVE(s)        decrement(s)
#define DS_SIZE(s)          counter_size(s)
#define DS_NEW(n,w,d,m,k)         create_counter(n,w,d,m,k)

#define DS_TYPE             counter_t
#define DS_NODE             index_t

/* Type definitions */
typedef struct file_descriptor
{
	uint64_t get_count;
	uint64_t put_count;
} descriptor_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct index_node
{
	descriptor_t descriptor;
	uint8_t padding[CACHE_LINE_SIZE - (sizeof(int64_t) + sizeof(uint64_t))];
} index_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct counter
{
	index_t *get_array;
	index_t *put_array;
	uint64_t random_hops;
	uint64_t depth;
	uint64_t width;
	uint64_t relaxation_bound;
	uint8_t k_mode;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(uint8_t) - (2 * sizeof(index_t*)) - (sizeof(int64_t)*4)];
} counter_t;

/*Global variables*/


/*Thread local variables*/
extern __thread ssmem_allocator_t* alloc;
extern __thread int thread_id;

extern __thread unsigned long my_put_cas_fail_count;
extern __thread unsigned long my_get_cas_fail_count;
extern __thread unsigned long my_null_count;
extern __thread unsigned long my_hop_count;
extern __thread unsigned long my_slide_count;

/* Interfaces */
uint64_t increment(counter_t *set);
uint64_t decrement(counter_t *set);
counter_t* create_counter(size_t num_threads, uint64_t width, uint64_t depth, uint8_t k_mode, uint64_t relaxation_bound);
size_t counter_size(counter_t *set);
int floor_log_2(unsigned int n);