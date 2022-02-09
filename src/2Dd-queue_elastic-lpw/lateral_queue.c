#include "lateral_queue.h"
#include "2Dd-window_elastic.h"


static void free_lateral_node(lateral_node_t* node)
{
	// Recycle node which was not pushed
	#if GC == 1
		ssmem_free(alloc, node);
	#endif
}


static lateral_node_t* create_lateral_node(row_t count, width_t width)
{
    lateral_node_t* node;

    #if GC == 1
		node = ssmem_alloc(alloc, sizeof(lateral_node_t));
	#else
	  	node = ssalloc(sizeof(lateral_node_t));
	#endif

	node->width = width;
	node->count = count;
	node->next = NULL;

	return node;
}


static void try_enq_lateral(lateral_queue_t* lateral, row_t count, width_t width)
{
	/* We are in a window with shifting width and want to push the old width */
	lateral_node_t* read_tail = lateral->tail;

	// Makes sure the node has not already been pushed
	if (read_tail->count < count){

		// First possibly enqueue the node
		lateral_node_t* next_tail = read_tail->next;
		if (next_tail == NULL) {
			lateral_node_t* new_node = create_lateral_node(count, width);

			if (!CAE(&read_tail->next, &next_tail, &new_node)) {
				// Recycle node which was not enqueued
				free_lateral_node(new_node);
			} else {
				next_tail = new_node;
			}

		}

		// Then update the lateral with the new node (next_tail)
		assert(next_tail != NULL);

		CAE(&lateral->tail, &read_tail, &next_tail);
	}

}


void maybe_enq_lateral(lateral_queue_t* lateral, row_t window_max, width_t width, width_t next_width)
{
	/* Ensures that the lateral is in a consistently defined state before shifting from a window */

	if (unlikely(width != next_width))
	{
		// Push a lateral to signify a change in width above the top of the window.
		try_enq_lateral(lateral, window_max + 1, next_width);
	}

}


width_t get_next_window_lateral(lateral_queue_t* lateral, row_t prev_max, row_t* next_max)
{
	lateral_node_t* current_node = lateral->head;
	lateral_node_t* lookahead = current_node->next;

	assert(current_node != NULL);
	assert(current_node->count <= *next_max);

	// Find the first node abobe prev max but below next max. If such a node exists (which it rarely does)
	// In the normal case we will not find it, and just find the first node below prev_max
	while (lookahead != NULL && lookahead->count <= prev_max + 1)	// So this will most likely not happen
	// The + 1 is to allow a clean cut, since it will naturally transition the width and max
	{
		current_node = lookahead;
		lookahead = lookahead->next;
	}
	// After this loop current_node should have the width of the previous window, and lookahead is potentially the next width change

	// Is there a change of width during the next planned window?
	if (unlikely(lookahead != NULL && lookahead->count <= *next_max))
	{
		*next_max = lookahead->count - 1; // Again -1 for the cleanness
	}

	width_t width = current_node->width;
	assert(width != 0);
	return width;

}

// Dequeues lateral nodes lower or equal to the specified count
void deq_lateral(lateral_queue_t* lateral, row_t max)
{
	// Tries to remove all possible nodes. Only needs to be called for GC
	lateral_node_t* read_head = lateral->head;

	lateral_node_t* new_head = read_head;
	lateral_node_t* probe = new_head->next;
	while (probe != NULL && probe->count <= max) // Not interesting to keep
	{
		new_head = probe;
		probe = probe->next;
	}

	if (read_head != new_head)
	{
		// Dequeue all nodes in one fell swoop
		CAE(&lateral->head, &read_head, &new_head);
	}
}


lateral_queue_t* create_lateral_queue(width_t width)
{
    lateral_queue_t* lateral;
    lateral_node_t* node;

    if ((lateral = ssalloc_aligned(CACHE_LINE_SIZE, sizeof(lateral_queue_t))) == NULL)
    {
		perror("malloc at allocating lateral queue");
		exit(1);
    }

    // Can't use create_node as alloc is not initialized for main thread
    node = ssalloc(sizeof(lateral_node_t));
    node->width = width;
	node->count = 0;
	node->next = NULL;

	lateral->head = lateral->tail = node;

    return lateral;
}
