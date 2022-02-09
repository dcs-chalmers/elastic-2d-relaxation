#ifndef LATERAL_QUEUE_H
#define LATERAL_QUEUE_H

#include <stdint.h>
#include "types.h"


/* Type definitions */

// Now the lateral node completely takes over the window
typedef struct lateral_node
{
	struct lateral_node* volatile next;
	// All nodes under this count are affected by it (so from below this count to the next one)
    row_t max;
	width_t width;
	depth_t depth;

	uint8_t padding[CACHE_LINE_SIZE - sizeof(struct lateral_node*) - sizeof(row_t) - sizeof(width_t) - sizeof(depth_t)];
} lateral_node_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct lateral_block
{
	// As we use a GC we don't have to worry about ABA.
	lateral_node_t* volatile tail;
	lateral_node_t* volatile head;	// A MS queue, so the head is actually the last_head (previously dequeued)
	uint8_t padding[CACHE_LINE_SIZE - 2*sizeof(lateral_node_t*)];
} lateral_queue_t;


/* Interfaces (new) */

// For shifting (enq or deq node)
void shift_put(lateral_queue_t* lateral, lateral_node_t* tail, row_t max, depth_t depth, width_t width);
void shift_get(lateral_queue_t* lateral, lateral_node_t* head);

// could have functions for peeking the lateral, but that we can just do with normal reads

// /* Interfaces */
// // Makes sure the lateral enqueues a node if put window changes size
// void maybe_enq_lateral(lateral_queue_t* lateral, row_t window_max, width_t width, width_t next_width);

// // Dequeues lateral nodes lower or equal to the specified count
// void deq_lateral(lateral_queue_t* lateral, row_t old_max);

// // Calcutates the width and max for the next window. If there is a change in the width, it will truncate the max to only contain one width
// width_t get_next_window_lateral(lateral_queue_t* lateral, row_t prev_max, row_t* next_max);

lateral_queue_t* create_lateral_queue(depth_t depth, width_t width);

#endif
