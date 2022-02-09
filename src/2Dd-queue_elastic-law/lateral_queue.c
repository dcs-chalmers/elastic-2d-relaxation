#include "lateral_queue.h"
#include "2Dd-window_elastic.h"


static void free_lateral_node(lateral_node_t* node)
{
	// Recycle node which was not pushed
	#if GC == 1
		ssmem_free(alloc, node);
	#endif
}


static lateral_node_t* create_lateral_node(row_t max, width_t width, depth_t depth)
{
    lateral_node_t* node;

    #if GC == 1
		node = ssmem_alloc(alloc, sizeof(lateral_node_t));
	#else
	  	node = ssalloc(sizeof(lateral_node_t));
	#endif

	node->width = width;
	node->max = max;
	node->depth = depth;
	node->next = NULL;

	return node;
}

void shift_put(lateral_queue_t* lateral, lateral_node_t* tail_pointer, row_t max, depth_t depth, width_t width) {
	// We just try to push a new node to the lateral, given that the last node is what we think

	if (likely(lateral->tail == tail_pointer)) {
		lateral_node_t* next = tail_pointer->next;
		if (next == NULL) {
			// Create the new node
			next = create_lateral_node(max, width, depth);
			node_t* expected = NULL;

			CAE(&tail_pointer->next, &expected, &next);
		}

		next = tail_pointer->next;
		CAE(&lateral->tail, &tail_pointer, &next);
	}
}

// Dequeues the head, if the head_pointer is correct
void shift_get(lateral_queue_t* lateral, lateral_node_t* head_pointer) {
	// We don't need to implement any helping here, as we know there is a lateral
	// node above when shifting up. (so we will never have to help).

	assert(head_pointer->next != NULL);
	CAE(&lateral->head, &head_pointer, &head_pointer->next);
}


lateral_queue_t* create_lateral_queue(depth_t depth, width_t width)
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
	node->max = 0;
	node->next = NULL;
	node->depth = 0;

	lateral->head = node;

    node = ssalloc(sizeof(lateral_node_t));
    node->width = width;
	node->max = depth;
	node->next = NULL;
	node->depth = depth;

	lateral->head->next = lateral->tail = node;

    return lateral;
}
