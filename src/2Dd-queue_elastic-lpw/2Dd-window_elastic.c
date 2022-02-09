
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

static inline depth_t put_depth(DS_TYPE* set) {
	#ifdef DIFF_DEPTH
	return set->put_depth;
	#else
	return set->depth;
	#endif

}

static inline row_t get_shift_max(DS_TYPE* set)
{
	// Returns the max of the new get window. Can't surpas the put windows max.

	#ifdef DIFF_DEPTH
	row_t max = thread_GWindow.max + set->get_depth;
	#else
	row_t max = thread_GWindow.max + set->depth;
	#endif


	if (max > thread_PWindow.max || max > global_PWindow.content.max)
	{
		// Slow, but almost empty, so not the most important
		max = global_PWindow.content.max;
	}

	return max;
}

// Reads the global pwindow into the thread local one atomically
static void read_pwindow()
{
	// Opt: Can we read it in two consecutive parts? First in that case the version, and then the rest
	__atomic_load(&global_PWindow.content, &thread_PWindow, __ATOMIC_SEQ_CST);
}

static void read_gwindow()
{
	// Opt: Can we read it in two consecutive parts? First in that case the version, and then the rest
	__atomic_load(&global_GWindow.content, &thread_GWindow, __ATOMIC_SEQ_CST);
}


descriptor_t put_window(DS_TYPE* set, uint8_t contention)
{
	width_t hops, random;
	descriptor_t descriptor;
	put_window_t new_window;
	uint64_t window_word1;

	hops = random = 0;

	window_word1 = global_PWindow.content.word1;
	if(thread_PWindow.word1 != window_word1)
	{
		thread_PWindow.word1 = window_word1;
		thread_PWindow.word2 = global_PWindow.content.word2; // as we read this strictly after word1 (x86 guarantee), it has to be at least as new (atomic read preferable, but scales horribly)
		// read_pwindow();
		assert(thread_PWindow.depth != 0);
	}

	// Can happen when you have synchronized width from somewhere else
	if (contention == 1 || unlikely(thread_put_index >= thread_PWindow.width)) {
		thread_put_index = random_index(thread_PWindow.width);
	}

	while(1)
	{
		//read descriptor
		descriptor =  set->put_array[thread_put_index].descriptor;

		// Read the global get window and possibly sync
		window_word1 = global_PWindow.content.word1;
		if(thread_PWindow.word1 != window_word1)
		{
			thread_PWindow.word1 = window_word1;
			thread_PWindow.word2 = global_PWindow.content.word2; // as we read this strictly after word1 (x86 guarantee), it has to be at least as new (atomic read preferable, but scales horribly)
			hops = 0;
			// read_pwindow();
			if (unlikely(thread_put_index >= thread_PWindow.width)) {
				thread_put_index = 0;
			}
		}

		// Valid index
		else if(descriptor.put_count < thread_PWindow.max)
		{
			assert(thread_put_index < thread_PWindow.width);
			if (likely(thread_GWindow.width == thread_PWindow.width)) {
				thread_get_index = thread_put_index;
			}
			return descriptor;
		}

		//hop
		else if(hops < thread_PWindow.width)
		{
			thread_put_index = hop(set, thread_put_index, &random, &hops, thread_PWindow.width);
		}

		//shift window
		else
		{
			// Could skip this
			if(thread_PWindow.max == global_PWindow.content.max)
			{
				maybe_enq_lateral(set->lateral, thread_PWindow.max, thread_PWindow.width, thread_PWindow.next_width);

				new_window.depth = put_depth(set);
				new_window.max = thread_PWindow.max + new_window.depth;
				new_window.next_width = set->width;
				new_window.width = thread_PWindow.next_width;

				if(CAE(&global_PWindow.content, &thread_PWindow, &new_window))
				{
					my_slide_count+=1;
					thread_PWindow = new_window;
				}
				else
				{
					// CAE wrote real value into window
				}
			}

			hops = 0;
			if (unlikely(thread_put_index >= thread_PWindow.width)) {
				thread_put_index = 0;
			}
		}
	}

}

descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	descriptor_t descriptor;
	get_window_t new_window;
	uint64_t window_word1;
	row_t put_count;
	width_t hops, random;
	uint8_t notempty;

	notempty = hops = random = 0;

	if(contention == 1)
	{
		thread_get_index = random_index(thread_GWindow.width);
		contention = 0;
	}

	// TODO remove? As we already have in loop. Benchmark
	window_word1 = global_GWindow.content.word1;
	if(thread_GWindow.word1 != window_word1)
	{
		thread_GWindow.word1 = window_word1;
		thread_GWindow.word2 = global_GWindow.content.word2; // as we read this strictly after word1 (x86 guarantee), it has to be at least as new (atomic read preferable, but scales horribly)
		// read_gwindow();
		if (unlikely(thread_get_index >= thread_GWindow.width)) {
			thread_get_index = 0;
		}
	}

	row_t put_count_lower_bound = thread_PWindow.max - thread_PWindow.depth;

	while(1)
	{

		//read descriptor
		descriptor =  set->get_array[thread_get_index].descriptor;

		if (put_count_lower_bound >= descriptor.get_count)
		{
			// We must be able to dequeue from the sub-queue, as it is not overlapping the current/old put window
			put_count = put_count_lower_bound;

		}
		else {
			// Here we must check if the sub-structure is actually empty, which has performance implications
			put_count = set->put_array[thread_get_index].descriptor.put_count;
		}


		// Read the global get window and possibly sync
		window_word1 = global_GWindow.content.word1;
		if(thread_GWindow.word1 != window_word1)
		{
			thread_GWindow.word1 = window_word1;
			thread_GWindow.word2 = global_GWindow.content.word2; // as we read this strictly after word1 (x86 guarantee), it has to be at least as new (atomic read preferable, but scales horribly)
			// read_gwindow();
			hops = notempty = 0;
			if (unlikely(thread_get_index >= thread_GWindow.width)) {
				thread_get_index = 0;
			}
		}

		// Valid return
		else if(descriptor.get_count < thread_GWindow.max && descriptor.get_count < put_count)
		{
			break;
		}

		// Hop
		else if (hops != thread_GWindow.width)
		{
			//notempty = notempty || descriptor.get_count < put_count;
			if (notempty == 0 && descriptor.get_count < put_count)
			{
				notempty = 1;
			}
			thread_get_index = hop(set, thread_get_index, &random, &hops, thread_GWindow.width);
		}

		// Shift window
		else if (notempty || thread_GWindow.max != global_PWindow.content.max)	// Checking the window so that there are no nodes diagonally outside current window
		{
			new_window.max = get_shift_max(set);
			new_window.width = get_next_window_lateral(set->lateral, thread_GWindow.max, &new_window.max);
			new_window.depth = new_window.max - thread_GWindow.max;

			if(thread_GWindow.max == global_GWindow.content.max)
			{
				assert(new_window.max >= thread_GWindow.max);		// Since the window is not empty, it must be possible to shift up
				if(CAE(&global_GWindow.content, &thread_GWindow, &new_window))
				{
					my_slide_count+=1;
					deq_lateral(set->lateral, new_window.max);
					thread_GWindow = new_window;
				}
				else
				{
					// CAE writes real window into thread window
				}
			}

			hops = notempty = 0;
			if (unlikely(thread_get_index >= thread_GWindow.width)) {
				thread_get_index = 0;
			}
		}

		// Empty return
		else
		{
			break;
		}
	}

	if (likely(thread_GWindow.width == thread_PWindow.width)) {
		thread_put_index = thread_get_index;
	}
	return descriptor;
}

width_t random_index(width_t width)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % width);
}

void initialize_global_window(depth_t depth, width_t width)
{
	/**
	 * Initializes the required global window variables
	 */

	assert(depth != 0);

	printf("Initializing! %zu %zu\n", depth, width);

	global_PWindow.content.max = depth;
	global_PWindow.content.width = width;
	global_PWindow.content.next_width = width;
	global_PWindow.content.depth = depth;

	global_GWindow.content.max = depth;
	global_GWindow.content.width = width;
	global_GWindow.content.depth = depth;
}
