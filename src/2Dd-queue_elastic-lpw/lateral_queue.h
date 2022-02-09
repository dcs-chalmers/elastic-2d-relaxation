#ifndef LATERAL_QUEUE_H
#define LATERAL_QUEUE_H

#include <stdint.h>


/* Type definitions */
typedef struct lateral_node
{
	struct lateral_node* volatile next;
	// All nodes under this count are affected by it (so from below this count to the next one)
    row_t count;
	width_t width;

	uint8_t padding[CACHE_LINE_SIZE - sizeof(struct lateral_node*) - sizeof(row_t) - sizeof(width_t)];
} lateral_node_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct lateral_block
{
	// As we use a GC we don't have to worry about ABA. Could still be good for cache locality
	lateral_node_t* volatile tail;
	lateral_node_t* volatile head;	// A MS queue, so the head is actually the last_head (previously dequeued)
	uint8_t padding[CACHE_LINE_SIZE - 2*sizeof(lateral_node_t*)];
} lateral_queue_t;


/* Interfaces */
// Makes sure the lateral enqueues a node if put window changes size
void maybe_enq_lateral(lateral_queue_t* lateral, row_t window_max, width_t width, width_t next_width);

// Dequeues lateral nodes lower or equal to the specified count
void deq_lateral(lateral_queue_t* lateral, row_t old_max);

// Calcutates the width and max for the next window. If there is a change in the width, it will truncate the max to only contain one width
width_t get_next_window_lateral(lateral_queue_t* lateral, row_t prev_max, row_t* next_max);

// Initializes queue so that the last/recently dequeued item is the base width
lateral_queue_t* create_lateral_queue(width_t width);

#endif
