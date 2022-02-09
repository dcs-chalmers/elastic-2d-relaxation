
#include "2Dd-window_elastic.h"
#include "2Dd-queue_elastic.h"
#include "lateral_queue.h"

#include "lateral_queue.c"


static inline uint64_t min(uint64_t x, uint64_t y)
{
	if (x <= y)
	{
		return x;
	}
	else
	{
		return y;
	}
}

static inline uint64_t max(uint64_t x, uint64_t y)
{
	if (x >= y)
	{
		return x;
	}
	else
	{
		return y;
	}
}


static inline width_t hop(DS_TYPE* set, width_t index, width_t* random, width_t* hops, width_t width)
{
	my_hop_count+=1;

	if(*random < set->random_hops)
	{
		*random += 1;
		index = random_index(width);
	}
	else
	{
		*hops += 1;
		index += 1;

		if(unlikely(index >= width))
		{
			index = 0;
		}
	}

	return index;

}

descriptor_t put_window(DS_TYPE* set, uint8_t contention)
{
	width_t hops, random;
	descriptor_t descriptor;
	lateral_queue_t* lateral = set->lateral;

	hops = random = 0;

	thread_put_window = thread_ltail_pointer = lateral->tail;
	if(contention == 1 || unlikely(thread_put_index >= thread_put_window->width))
	{
		thread_put_index = random_index(thread_put_window->width);
		contention = 0;
	}

	while(1)
	{
		//read descriptor
		descriptor =  set->put_array[thread_put_index].descriptor;

		// Read the global put window and possibly sync
		lateral_node_t* lat_tail = lateral->tail;
		if(lat_tail != thread_ltail_pointer)
		{
			thread_ltail_pointer = lat_tail;
			thread_put_window = lat_tail;
			hops = 0;
			if (unlikely(thread_put_index >= thread_put_window->width)) {
				thread_put_index = 0;
			}
		}

		// Valid index
		else if(descriptor.put_count < thread_put_window->max)
		{
			assert(thread_put_index < thread_put_window->width);
			if (thread_get_window && likely(thread_get_window->width == thread_put_window->width)) {
				// TODO: Not efficient check in producer/consumer workloads
				thread_get_index = thread_put_index;
			}
			return descriptor;
		}

		//hop
		else if(hops < thread_put_window->width)
		{
			thread_put_index = hop(set, thread_put_index, &random, &hops, thread_put_window->width);
		}

		//shift window
		else
		{
			// Could skip this
			if(thread_ltail_pointer == lateral->tail)
			{
				depth_t depth = set->depth;
				shift_put(lateral, thread_ltail_pointer, thread_put_window->max + depth, depth, set->width);
			}

			thread_ltail_pointer = lateral->tail;
			thread_put_window = thread_ltail_pointer;
			hops = 0;
			if (unlikely(thread_put_index >= thread_put_window->width)) {
				thread_put_index = 0;
			}
		}
	}

}

descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	descriptor_t descriptor;
	row_t put_count;
	width_t hops, random;
	lateral_queue_t* lateral = set->lateral;

	hops = random = 0;

	if(contention == 1)
	{
		thread_get_index = random_index(thread_get_window->width);
		contention = 0;
	}

	// TODO remove? As we already have in loop. Benchmark
	lateral_node_t* lat_head = lateral->head;
	if(lat_head != thread_lhead_pointer && likely(lat_head->next != NULL))
	{
		// This should always be safe (!= null)
		thread_lhead_pointer = lat_head;

		// Should we set this to null if it is?
		thread_get_window = lat_head->next;

		if (unlikely(thread_get_index >= thread_get_window->width)) {
			thread_get_index = 0;
		}
	}

	while(1)
	{

		//read descriptor
		descriptor =  set->get_array[thread_get_index].descriptor;

		// Check put count, to see that the queue is not empty. (this is not nice, would be better with segment implementation)
		if (thread_put_window->max > thread_get_window->max)
		{
			put_count = thread_put_window->max - thread_put_window->depth;
		}
		else
		{
			put_count = set->put_array[thread_get_index].descriptor.put_count;
		}

		// Read the global get window and possibly sync
		lateral_node_t* lat_head = lateral->head;
		if(lat_head != thread_lhead_pointer)
		{
			if (unlikely(lat_head->next == NULL)) {
				// Empty return, very rare for lateral to be empty
				break;
			}

			// This should always be safe (!= null)
			thread_lhead_pointer = lat_head;

			// Should we set this to null if it is?
			thread_get_window = lat_head->next;

			if (unlikely(thread_get_index >= thread_get_window->width)) {
				thread_get_index = 0;
			}
		}

		// Valid return
		else if(descriptor.get_count < thread_get_window->max && descriptor.get_count < put_count)
		{
			break;
		}

		// Hop
		else if (hops != thread_get_window->width)
		{
			thread_get_index = hop(set, thread_get_index, &random, &hops, thread_get_window->width);
		}

		// Shift window
		else if (thread_get_window != lateral->tail)	// Checking the window so that there are no nodes diagonally outside current window
		{
			assert(__atomic_load_n(&thread_get_window->next, __ATOMIC_SEQ_CST) != NULL);

			shift_get(lateral, thread_lhead_pointer);

			// We can do this safely, as we know that there was a window above the last one, so we must be in a window now
			thread_lhead_pointer = lateral->head;
			thread_get_window = thread_lhead_pointer->next;
			assert(thread_get_window != NULL);

			hops =  0;
			if (unlikely(thread_get_index >= thread_get_window->width)) {
				thread_get_index = 0;
			}
		}

		// Empty return
		else
		{
			break;
		}
	}

	if (thread_put_window && likely(thread_get_window->width == thread_put_window->width)) {
		// TODO: Not efficient in prod/cons settings
		thread_put_index = thread_get_index;
	}
	return descriptor;
}

width_t random_index(width_t width)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % width);
}

void init_thread_windows(DS_TYPE* set) {
	thread_lhead_pointer = set->lateral->head;
	thread_ltail_pointer = set->lateral->tail;

	thread_get_window = thread_lhead_pointer->next;
	thread_put_window = thread_ltail_pointer;
}
