#include "2Dd-queue_elastic.h"
#include "2Dd-window_elastic.c"

#ifdef RELAXATION_ANALYSIS
#include "relaxation_analysis_queue.c"
#endif

#ifdef ELASTIC_CONTROLLER
	#include "controller.h"
	__thread elastic_controller_t controller;
#else
	#define dec_put_controller(...) (false)
	#define inc_put_controller(...) (false)
	#define dec_get_controller(...) (false)
	#define inc_get_controller(...) (false)
#endif



RETRY_STATS_VARS;

#include "latency.h"

#if LATENCY_PARSING == 1
__thread size_t lat_parsing_get = 0;
__thread size_t lat_parsing_put = 0;
__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

void free_node(node_t* node)
{
	#if GC == 1
		ssmem_free(alloc, (void*) node);
	#endif
}

node_t* create_node(skey_t key, sval_t val, node_t* next)
{
	#if GC == 1
	  	node_t* node = ssmem_alloc(alloc, sizeof(node_t));
	#else
	  	node_t* node = ssalloc(sizeof(node_t));
	#endif

	assert(node != NULL);

	node->key = key;
	node->val = val;
	node->next = next;

	#ifdef __tile__
	  MEM_BARRIER;
	#endif

  return node;
}

mqueue_t* create_queue(size_t num_threads, width_t width, depth_t depth, width_t max_width, uint8_t k_mode, uint64_t relaxation_bound)
{
	// Creates the data structure, including windows

	mqueue_t *set;

	/**** calculate width and depth using the relaxation bound ****/
	if(k_mode == 3)
	{
		//maximum width is fixed as a multiple of number of threads
		width = num_threads * width;
		if(width < 2 )
		{
			width  = 1;
			depth  = relaxation_bound;
			relaxation_bound = 0;
		}
		else
		{
			depth = relaxation_bound / (width - 1);
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / depth) + 1;
			}
		}
	}
	else if(k_mode == 2)
	{
		//maximum depth is fixed
		width = (relaxation_bound / depth) + 1;
		if(width<1)
		{
			width = 1;
			depth  = relaxation_bound;
			relaxation_bound = 0;
		}
	}
	else if(k_mode == 1)
	{
		//width parameter is fixed
		if(width < 2 )
		{
			width  = 1;
			depth  = relaxation_bound;
			relaxation_bound = 0;
		}
		else
		{
			depth = relaxation_bound / (width - 1);
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / depth) + 1;
			}
		}
	}
	else if(k_mode == 0)
	{
		relaxation_bound = depth * (width -1);
	}
	/*************************************************************/

	if ((set = (mqueue_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(mqueue_t))) == NULL)
	{
	  perror("malloc");
	  exit(1);
	}

	if (max_width < width)
	{
		max_width = width;
	}

	width_t allocated_width = max_width;

	// Initialize all descriptors to zero (empty)
	set->get_array = (volatile index_t*)ssalloc_aligned(CACHE_LINE_SIZE, allocated_width*sizeof(index_t));
	set->put_array = (volatile index_t*)ssalloc_aligned(CACHE_LINE_SIZE, allocated_width*sizeof(index_t));
	set->lateral = create_lateral_queue(depth, width);
	set->random_hops = 2;
	#ifdef DIFF_DEPTHS
	set->get_depth = depth;
	set->put_depth = depth;
	#else
	set->depth = depth;
	#endif
	set->width = width;
	set->max_width = max_width;
	set->k_mode = k_mode;
	set->relaxation_bound = relaxation_bound;

	// Don't need anymore as we only have the lateral to initiate
	// Initlialize the window variables
	// initialize_global_window(depth, width);

	uint64_t i;
	node_t *node;
	for(i = 0; i < allocated_width; i++)
	{
		// Initialize with dummy nodes
		//node = create_node(0, 0, NULL);	// This thread isn't properly initialized
		node = (node_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(node_t));
		if (node == NULL)
			printf("ERROR: Memory ran out when allocating queue");
		node->next = NULL;

		set->put_array[i].descriptor.node = set->get_array[i].descriptor.node = node;
		set->put_array[i].descriptor.put_count = 0;
		set->get_array[i].descriptor.get_count = 0;
	}

	return set;
}


static int enq_cae(node_t** next_loc, node_t* new_node)
{
	node_t* expected = NULL;
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();

	if (CAE(next_loc, &expected, &new_node))
	{
		new_node->val = gen_relaxation_count();
		add_linear(new_node->val, 0);
		unlock_relaxation_lists();
		return true;
	}
	else {
		unlock_relaxation_lists();
		return false;
	}

#else
	return CAE(next_loc, &expected, &new_node);
#endif
}

static int deq_cae(descriptor_t* des_loc, descriptor_t* read_des_loc, descriptor_t* new_des_loc)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();
	if (CAE(des_loc, read_des_loc, new_des_loc))
	{
		remove_linear(new_des_loc->node->val);
		unlock_relaxation_lists();
		return true;
	}
	else {
		unlock_relaxation_lists();
		return false;
	}

#else
	return CAE(des_loc, read_des_loc, new_des_loc);
#endif
}


int enqueue(mqueue_t* set, skey_t key, sval_t val, int no_init)
{
	node_t* tail;
	uint8_t contention = 0;
	descriptor_t descriptor, new_descriptor;

	node_t* new_node = create_node(key, val, NULL);
	// printf("enqueue\n");
	while(1)
  {

		descriptor = put_window(set,contention);
		assert(descriptor.node != NULL);
		// assert(thread_put_window->max >= thread_get_window->max);
		assert(descriptor.put_count < thread_put_window->max);

		tail = descriptor.node; // Use tail->count instead of descriptor->count, as the descriptor can have the wrong count (non-atomic read)
		// row_t curr_count = tail->count;
		// ERR: Is this an error in the original algorithm?
		if (set->put_array[thread_put_index].descriptor.get_count >= thread_put_window->max) {
			continue;
		}

		new_descriptor.node = new_node;
		assert(new_descriptor.node != NULL);

		// Not very nice to bleed window information here either
		if (likely(descriptor.put_count >= thread_put_window->max - thread_put_window->depth))
		{
			new_descriptor.put_count = descriptor.put_count + 1;
		}
		else
		{
			new_descriptor.put_count = thread_put_window->max - thread_put_window->depth + 1;
		}
		// new_node->count = new_descriptor.put_count;


		if(tail->next == NULL) // If null pointer error, check we have initiated the ms queues
		{
			if(enq_cae(&tail->next, new_node))
			{
				// Linearization of the enqueue, enqueing the node.
				// assert(new_node->count <= thread_PWindow.max);
				assert(thread_put_index <= thread_put_window->width);
				if (likely(no_init)) dec_put_controller(&controller, set, thread_put_window);

				break;
			}
			else
			{
				contention = 1;
				if (likely(no_init)) inc_put_controller(&controller, set, thread_put_window);
			}
		}
		else
		{
			// Try helping pending enqueue
			// From the same descriptor, so it must be the same count
			new_descriptor.node = tail->next;

			if(!CAE(&set->put_array[thread_put_index].descriptor, &descriptor, &new_descriptor))
			{
				contention = 1;
			}
			else
			{
				// I think this assert has actually failed a couple of times
				assert(new_descriptor.put_count <= thread_put_window->max);
				// assert(new_descriptor.node->count == new_descriptor.put_count);	// Should come to the same conclusion
			}
		}
		my_put_cas_fail_count+=1;
  }

	assert(new_descriptor.node != NULL);
	CAE(&set->put_array[thread_put_index].descriptor, &descriptor, &new_descriptor);

	return 1;
}


sval_t dequeue(mqueue_t* set, int no_init)
{
	sval_t val;
	node_t *head;
	uint8_t contention = 0;
	descriptor_t enq_descriptor, new_enq_descriptor, deq_descriptor, new_deq_descriptor;


	thread_put_window = thread_ltail_pointer = set->lateral->tail;

	while (1)
    {
		deq_descriptor = get_window(set,contention);

		head = deq_descriptor.node;

		if (thread_put_window->max > thread_get_window->max)
		{
			// Heuristic tells us we are not dequeuing the top item in the queue, so don't have to check tail => faster operation
			goto safe_deq;
		}

		enq_descriptor = set->put_array[thread_get_index].descriptor;
		node_t* tail = enq_descriptor.node;

		if (unlikely(head == tail))	// Empty, or close to it
		{
			if(head->next == NULL)
			{
				my_null_count+=1;
				return 0;
			}
			else
			{
				// Try helping pending enqueue
				new_enq_descriptor.node = tail->next;
				lateral_node_t* lat_tail = set->lateral->tail;
				row_t enq_win_bottom = lat_tail->max - lat_tail->depth;
				if (likely(enq_descriptor.get_count >= enq_win_bottom))
				{
					new_enq_descriptor.put_count = enq_descriptor.get_count + 1;
				}
				else
				{
					new_enq_descriptor.put_count = enq_win_bottom + 1;
				}
				// new_enq_descriptor.put_count = new_enq_descriptor.node->count;

				if(!CAE(&set->put_array[thread_get_index].descriptor, &enq_descriptor, &new_enq_descriptor))
				{
					contention = 1;
					// TODO: Which controller should we increment here? This basically just happens when we have too few items.
					// if (likely(no_init)) inc_put_controller(&controller);
				}
				else {
					// if (likely(no_init)) dec_put_controller(&controller);
				}
			}
		}
		else	// Can dequeue without worrying about tail
		{
			safe_deq:
			new_deq_descriptor.node = head->next;

			new_deq_descriptor.get_count = deq_descriptor.get_count + 1;
			row_t min = thread_get_window->max - thread_get_window->depth + 1; // The node cannot be dequeued to below the window, as it then should have been dequeus last window. And width can only change at boundaries, so the gap must have stopped at min.
			if (unlikely(new_deq_descriptor.get_count < min))
			{
				// Gaps must close at the bottom of the window
				new_deq_descriptor.get_count = min;
			}


			if(deq_cae(&set->get_array[thread_get_index].descriptor, &deq_descriptor, &new_deq_descriptor))
			{
				free_node(head);
				if (likely(no_init)) dec_get_controller(&controller, set, thread_get_window);
				return new_deq_descriptor.node->val;
			}
			else
			{
				contention = 1;
				my_get_cas_fail_count+=1;
				if (likely(no_init)) inc_get_controller(&controller, set, thread_get_window);
			}
		}
    }
}


size_t queue_size(mqueue_t *set)
{
	size_t size = 0;
	uint64_t q = 0;
	node_t *head, *tail;

	while(q < set->max_width)
	{
		head = set->get_array[q].descriptor.node;
		tail = set->put_array[q].descriptor.node;
		while (head!=tail)	// is this correct? What about halfway done ones?
		{
			head = head->next;
			size+=1;
		}
		q++;
	}
	return size;
}


void validate_structure(mqueue_t *set)
{
	/**
	 * Checks if the window and sub-structures seem to match.
	 *
	 * TODO: Update, and make correct again.
	 *
	 * 1) Checks that no count has exceeded the windows.
	 * 2) Checks if any count seems to be below window (can't really be certain this means a bug)
	 * 3) Checks if put window has the same active width as the global width
	 * 	3.1) Also checks that the active ones do not have gaps
	 * 	3.2) And that the first non-active one has a gap
	 * 4) Checks that the read window has not missed expanding
	 * 	4.1) Also that it is not too large
	 */


	width_t width = set->width;
	depth_t depth = set->depth;


	lateral_node_t* lat_tail = set->lateral->tail;
	row_t put_max = lat_tail->max;
	for (int i = 0; i < lat_tail->width; i++) // TOOD: is this correct?
	{
		descriptor_t des = set->put_array[i].descriptor;

		// Check that they all have ok put counts
		if (des.put_count > put_max)
			printf("ERROR: Too high put count at index %d\n", i);
	}

	lateral_node_t* lat_head = set->lateral->head;
	width_t get_width = lat_head->width;
	row_t get_max = lat_head->max;
	for (int i = 0; i < get_width; i++)
	{
		descriptor_t des = set->get_array[i].descriptor;

		// Check that they all have ok get counts
		if (des.get_count > get_max)
			printf("WARNING: Too high get count (%zu/%zu) at index %d\n", des.get_count, get_max, i);

		if (des.get_count < get_max - depth)
			printf("WARNING: Too low get count (%zu/%zu) at index %d\n", des.get_count, get_max - depth, i);

	}
}

#ifndef DIFF_DEPTHS
depth_t update_depth(mqueue_t *set, depth_t depth)
{
	/**
	 * Changes the depth of the future global windows. Returns old one
	 */

	printf("Updating depth to %d\n", depth);
	return SWP(&set->depth, depth);
}
#else
depth_t update_put_depth(mqueue_t *set, depth_t depth)
{
	/**
	 * Changes the depth of the future global put windows. Returns old one
	 */

	return SWP(&set->put_depth, depth);
}

depth_t update_get_depth(mqueue_t *set, depth_t depth)
{
	/**
	 * Changes the depth of the future global get windows. Returns old one
	 */

	return SWP(&set->get_depth, depth);
}
#endif

width_t update_width(mqueue_t *set, width_t width)
{
	/**
	 * Changes the width of the next global put window. Returns old one
	 */

	// printf("Setting width to %d\n", width);
	return SWP(&set->width, width);

}

depth_t get_depth(mqueue_t *set)
{
	return set->depth;
}

width_t get_width(mqueue_t *set)
{
	return set->width;
}

row_t get_put_width()
{
	// return thread_put_window->max;
	return thread_put_window->width;
}

row_t get_get_width()
{
	// return thread_get_window->max;
	return thread_get_window->width;
}
