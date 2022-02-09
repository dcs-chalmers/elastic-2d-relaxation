
#include "2Dd-window_optimized.h"
#include "2Dd-queue_optimized.h"

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
	window_t new_window;

	hops = random = 0;

	if(contention == 1)
	{
		thread_put_index = random_index(thread_width);
		contention = 0;
	}

	thread_PWindow.max = global_PWindow.content.max;

	while(1)
	{
		//read descriptor
		descriptor =  set->put_array[thread_put_index].descriptor;

		// Read the global get window and possibly sync
		row_t gmax = global_PWindow.content.max;
		if(thread_PWindow.max != gmax)
		{
			thread_PWindow.max = gmax;
			hops = 0;
		}

		// Valid index
		else if(descriptor.put_count < thread_PWindow.max)
		{
			thread_get_index = thread_put_index;
			return descriptor;
		}

		//hop
		else if(hops < thread_width)
		{
			thread_put_index = hop(set, thread_put_index, &random, &hops, thread_width);
		}

		//shift window
		else
		{
			// Could skip this
			if(thread_PWindow.max == global_PWindow.content.max)
			{
				new_window.max = thread_PWindow.max + thread_depth;
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
		}
	}

}

descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	descriptor_t descriptor;
	window_t new_window;
	row_t put_count;
	width_t hops, random;
	uint8_t notempty;

	notempty = hops = random = 0;

	if(contention == 1)
	{
		thread_get_index = random_index(thread_width);
		contention = 0;
	}

	thread_GWindow.max = global_GWindow.content.max;

	while(1)
	{

		//read descriptor
		descriptor =  set->get_array[thread_get_index].descriptor;
		if (thread_GWindow.max < thread_PWindow.max) {
			put_count = thread_PWindow.max - thread_depth;
		}
		else {
			put_count = set->put_array[thread_get_index].descriptor.put_count;
		}
		// Read the global get window and possibly sync
		row_t gmax = global_GWindow.content.max;
		if(thread_GWindow.max != gmax)
		{
			thread_GWindow.max = gmax;
			hops = notempty = 0;
		}

		// Valid return
		else if(descriptor.get_count < thread_GWindow.max && descriptor.get_count < put_count)
		{
			break;
		}

		// Hop
		else if (hops != thread_width)
		{
			//notempty = notempty || descriptor.get_count < put_count;
			if (notempty == 0 && descriptor.get_count < put_count)
			{
				notempty = 1;
			}
			thread_get_index = hop(set, thread_get_index, &random, &hops, thread_width);
		}

		// Shift window
		else if (notempty || thread_GWindow.max != global_PWindow.content.max)	// Checking the window so that there are no nodes diagonally outside current window
		{
			new_window.max = thread_GWindow.max + thread_depth;

			if(thread_GWindow.max == global_GWindow.content.max)
			{
				if(CAE(&global_GWindow.content, &thread_GWindow, &new_window))
				{
					my_slide_count+=1;
					thread_GWindow = new_window;
				}
				else
				{
					// CAE writes real window into thread window
				}
			}

			hops = notempty = 0;
		}

		// Empty return
		else
		{
			break;
		}
	}

	thread_put_index = thread_get_index;
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
	global_GWindow.content.max = depth;
}

void ds_thread_init(DS_TYPE* set) {
	thread_depth = set->depth;
	thread_width = set->width;
}
