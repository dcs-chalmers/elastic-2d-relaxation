#ifndef TWODd_queue_elastic
#define TWODd_queue_elastic

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
#include <atomic_ops.h>	.
#include "lock_if.h"
#include "ssmem.h"
#include "utils.h"
#include "types.h"
#include "lateral_queue.h"


 /* ################################################################### *
	* Definition of macros: per data structure
* ################################################################### */

#define DS_ADD(s,k,v)       enqueue(s,k,v,true)
#define DS_REMOVE(s)        dequeue(s, true)
#define DS_SIZE(s)          queue_size(s)
#define DS_NEW(n,w,d,b,m,k)   create_queue(n,w,d,b,m,k)

#define DS_TYPE             mqueue_t
#define DS_NODE             node_t

/* Type definitions */

typedef struct mqueue_node
{
	skey_t key;
	sval_t val;

	// The count of the descriptor when this node was pushed
	// row_t count;
	struct mqueue_node* volatile next;

	uint8_t padding[CACHE_LINE_SIZE - 2*sizeof(skey_t) - sizeof(struct mqueue_node*) - sizeof(row_t)];
} node_t;

typedef struct file_descriptor
{
	node_t* node;
	union
	{
		row_t put_count;
		row_t get_count;
	};
} descriptor_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct array_index
{
	volatile descriptor_t descriptor;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(descriptor_t)];
} index_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct mqueue_file
{
	// Contains all constant information about the data structure
	index_t *get_array;
	index_t *put_array;
	lateral_queue_t* lateral;
	uint64_t random_hops;
	uint64_t relaxation_bound;	// Is not updated by elastic changes

	// Can access either depth, or put/get_depth. By default, put_depth = depth
	union {
	    volatile depth_t depth;
		struct {
		    volatile depth_t put_depth;
		    volatile depth_t get_depth;
		};
	};
	volatile width_t width;
	width_t max_width;
	uint8_t k_mode;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(uint8_t) - 3*sizeof(void*) - 2*sizeof(uint64_t) - 2*sizeof(depth_t) - 2*sizeof(width_t)];
} mqueue_t;

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
int enqueue(mqueue_t *set, skey_t key, sval_t val, int no_init);
sval_t dequeue(mqueue_t *set, int no_init);
mqueue_t* create_queue(size_t num_threads, width_t width, depth_t depth, width_t max_width, uint8_t k_mode, uint64_t relaxation_bound);
size_t queue_size(mqueue_t *set);
int floor_log_2(unsigned int n);

// Mainly for internal use
node_t* create_node(skey_t key, sval_t val, node_t* next);
void free_node(node_t* node);

// Elasticity
depth_t update_depth(mqueue_t *set, depth_t depth);
depth_t update_depth_put(mqueue_t *set, depth_t depth); // For only updating the depth on one side
depth_t update_depth_get(mqueue_t *set, depth_t depth); // For only updating the depth on one side
width_t update_width(mqueue_t *set, width_t width);
depth_t get_depth(mqueue_t *set);
width_t get_width(mqueue_t *set);
width_t get_put_width();
width_t get_get_width();

#endif
