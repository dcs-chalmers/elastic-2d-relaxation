#include "lateral_stack.h"
#include "2Dc-window_elastic.h"

static void free_lateral_node(lateral_node_t *node)
{
// Recycle node which was not pushed
#if GC == 1
	ssmem_free(alloc, node);
#endif
}

static lateral_node_t *create_lateral_node(lateral_node_t *next, row_t next_count, row_t width)
{
	// GC = 1 required access to alloc which is for the ssmem library. We have a strange source code structure though.
	lateral_node_t *node;

#if GC == 1
	node = ssmem_alloc(alloc, sizeof(lateral_node_t));
#else
	node = ssalloc(sizeof(lateral_node_t));
#endif

	node->width = width;
	node->next_count = next_count;
	node->next = next;

	return node;
}

static void free_nodes_until(lateral_node_t* node, lateral_node_t* base_node)
{
	if (node != base_node && node != NULL)
	{
		free_nodes_until(node->next, base_node);
		free_lateral_node(node);
	}
}

/* Counts where to lower or push lateral nodes depending on width as compared to put_width */
static inline row_t push_wider_count(descriptor_t* substructures)
{
	// Instead of having this in window we can just loop and get an upper bound
	row_t max = 0;
	for (width_t i = thread_Window.put_width; i < thread_Window.old_put_width; i += 1) 
	{
		row_t count = substructures[i].count;
		if (unlikely(count > max)) {
			max = count;
		}
	} 
	return max;
}

static inline row_t push_narrower_count()
{
	return thread_Window.max - thread_Window.depth + 1;
}

// static inline row_t lower_wider_count()
// {
// 	return thread_Window.potential_old_bottom;
// }

static inline row_t lower_narrower_count()
{
	return thread_Window.max - thread_Window.depth + 1;
}

static row_t lateral_new_count(row_t lateral_count, row_t lower_limit, width_t lateral_width, width_t put_width)
{
	// Get the new count for a lateral node given its width and the active width when lowering
	row_t new_count;

	if (lateral_width > put_width && thread_Window.last_shift != 0)
	{
		// shifted down last, so what must the last bottom have been?
		row_t last_bottom = thread_Window.max - (thread_Window.depth >> 1);
		if (last_bottom < lateral_count) {
			new_count = last_bottom;
		} else {
			new_count = lateral_count;
		}
	}
	else if (lateral_width <= put_width && lateral_count > lower_limit)
	{
		new_count = lower_limit;
	}
	else
	{
		new_count = lateral_count;
	}

	return new_count;
}

// To return a tuple in func below
typedef struct update_tuple {
	row_t count;
	lateral_node_t* node;	
	lateral_node_t* base; 			// The one in common between read and new stack
} update_tuple_t;

static update_tuple_t replace_lateral_node(lateral_node_t* node, row_t count)
{
	// Are we below the point any node can be lowered?
	if (likely(count < lower_narrower_count()))
	{
		// Don't do anything, set as base node in case nothing is changed
		update_tuple_t base = {count, node, node};
		return base;
	}

	update_tuple_t next = replace_lateral_node(node->next, node->next_count);
 	update_tuple_t replacement = next;

	row_t new_count = lateral_new_count(count, lower_narrower_count(), node->width, thread_Window.put_width);

	if (next.node == node->next && next.count == node->next_count && new_count == count) 
	{
		// Don't move this or anything below
		replacement.base = node;
		replacement.node = node;
		replacement.count = count;
		return replacement;
	}
	else if (next.node == node->next && next.count == node->next_count && new_count <= node->next_count) 
	{
		// Remove the node as its domain is now empty
		return next;
	}
	else if (next.node == node->next && next.count == node->next_count && new_count < count) 
	{
		// Just lower the node
		replacement.node = node;
		replacement.count = new_count;
		replacement.base = node;
		return replacement;
	}
	else if (new_count <= node->next_count)	// Nodes below have been changed
	{
		// Remove the node
		return next;
	}
	else 
	{
		// Replace node with a new copy
		replacement.node = create_lateral_node(next.node, next.count, node->width);
		replacement.count = new_count;
		return replacement;
	}
}

static void replace_laterals(lateral_descriptor_t* read_descriptor, lateral_descriptor_t* new_descriptor, lateral_node_t** base_node)
{
	// Lowers and replaces the nodes until base node, and adds new top to the new descriptor

	// In a way we don't need to calculate the base pointer in advance...
	update_tuple_t top = replace_lateral_node(read_descriptor->node, read_descriptor->count);

	new_descriptor->count = top.count;
	new_descriptor->node = top.node;
	*base_node = top.base;
}

static void push_lateral(lateral_descriptor_t* descriptor, row_t count, width_t width)
{
	descriptor->node = create_lateral_node(descriptor->node, descriptor->count, width);
	descriptor->count = count;
}

static void maybe_push_lateral(lateral_descriptor_t *new_descriptor, descriptor_t* substructures)
{
	/* We are in a window with shifting width and want to push the old width */
	
	if (thread_Window.put_width > thread_Window.old_put_width && 
		new_descriptor->count < push_narrower_count())
	{
		// Increasing width so push lateral at window bottom
		push_lateral(new_descriptor, push_narrower_count(), thread_Window.old_put_width);
	}
	else if (thread_Window.put_width < thread_Window.old_put_width)
	{
		// Decreasing width so push lateral at upper bound of where nodes can still be at that width
		row_t upper_bound = push_wider_count(substructures);
		if (new_descriptor->count < upper_bound)
		{
			push_lateral(new_descriptor, upper_bound, thread_Window.old_put_width);
		}
	}
}

void synchronize_lateral(lateral_stack_t *lateral, descriptor_t* substructures)
{
	/* ENsures that the lateral is in a consistently defined state before shifting from a window */
	lateral_descriptor_t read_descriptor;

	read_descriptor = lateral->descriptor;

	if (global_Window.content.version != thread_Window.version) {
		// To make sure this lateral stack was observed during the global window
		return ;
	}

	if (unlikely(read_descriptor.version == thread_Window.version))
	{
		// The descriptor has already been updated during this window, so don't do it twice!
		return ;
	}

	// Not yet updated the lateral, so try do it with one CAS from read_des to new_des
	lateral_descriptor_t new_descriptor;
	lateral_node_t *base_node; // The uppermost node which not to replace (can still be moved by updating counts above it)

	// Replace the required nodes in the new descriptor with new ones
	replace_laterals(&read_descriptor, &new_descriptor, &base_node);

	// Push a new lateral node if we have changed width
	if (unlikely(thread_Window.put_width != thread_Window.old_put_width))
	{
		maybe_push_lateral(&new_descriptor, substructures);
	}

	// Only do CAS if there is any change
	if (unlikely(
			new_descriptor.count != read_descriptor.count ||
			new_descriptor.node != read_descriptor.node
		))
	{
		new_descriptor.version = thread_Window.version;
		if (CAE(&lateral->descriptor, &read_descriptor, &new_descriptor))
		{
			// Managed to update it, so now the replaced nodes should be freed
			free_nodes_until(read_descriptor.node, base_node);
		}
		else
		{
			// Someone else managed to update it before us, so we need to free the created nodes
			free_nodes_until(new_descriptor.node, base_node);
		}
	}
}

lateral_stack_t *create_lateral_stack(width_t max_width)
{
	lateral_stack_t *set;
	lateral_node_t *node;

	if ((set = ssalloc_aligned(CACHE_LINE_SIZE, sizeof(lateral_stack_t))) == NULL)
	{
		perror("malloc at allocating lateral stack");
		exit(1);
	}

	// Can't use create_node as alloc is not initialized for main thread
	node = ssalloc(sizeof(lateral_node_t));
	node->width = max_width + 1;
	node->next_count = 0;
	node->next = NULL;

	set->descriptor.version = 0;
	set->descriptor.count = 0;
	set->descriptor.node = node;

	return set;
}
