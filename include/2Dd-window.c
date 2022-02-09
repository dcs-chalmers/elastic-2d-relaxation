
#include "2Dd-window.h"


static inline uint64_t hop(DS_TYPE* set, uint64_t index, uint64_t* random, uint64_t* hops)
{
	my_hop_count+=1;

	if(*random < set->random_hops)
	{
		*random += 1;
		index = random_index(set->width);
	}
	else
	{
		*hops += 1;
		index += 1;

		if(unlikely(index >= set->width))
		{
			index = 0;
		}
	}

	return index;

}


descriptor_t put_window(DS_TYPE* set, uint8_t contention)
{
	uint64_t hops, random;
	descriptor_t descriptor;
	put_window_t new_window, read_global_PWindow;

	hops = random = 0;

	if(contention == 1)
	{
		thread_index = random_index(set->width);
		contention = 0;
	}

	if(thread_PWindow.max != global_PWindow.content.max)
	{
		thread_PWindow = global_PWindow.content;
	}

	while(1)
	{

		//read descriptor
		assert(thread_index < set->width);
		// Not an atomic read, but it is ok since both parts are monotonically increasing and we only act on it with CAS
		descriptor =  set->put_array[thread_index].descriptor;
		// __atomic_load(&set->put_array[thread_index].descriptor, &descriptor, __ATOMIC_RELAXED);

		// Read the global get window and possibly sync
		read_global_PWindow = global_PWindow.content;
		if (read_global_PWindow.max != thread_PWindow.max)
		{
			thread_PWindow = read_global_PWindow;
			hops = 0;
		}

		// Return the valid descriptor
		else if(descriptor.put_count < read_global_PWindow.max)
		{
			return descriptor;
		}

		//hop
		else if(hops != set->width)
		{
			thread_index = hop(set, thread_index, &random, &hops);
		}

		//shift window
		else
		{
			if(thread_PWindow.max == global_PWindow.content.max)
			{
				new_window.max = thread_PWindow.max + set->depth;

				if(CAE(&global_PWindow.content, &thread_PWindow, &new_window))
				{
					my_slide_count+=1;
				}
			}

			thread_PWindow = global_PWindow.content;
			hops = 0;
		}
	}
}

descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	descriptor_t descriptor;
	get_window_t new_window, read_global_GWindow;
	uint64_t hops, random, put_count;
	uint8_t notempty;

	notempty = hops = random = 0;
	thread_GWindow = global_GWindow.content;

	if(contention == 1)
	{
		thread_index = random_index(set->width);
		contention = 0;
	}

	if(thread_GWindow.max != global_GWindow.content.max)
	{
		thread_GWindow = global_GWindow.content;
		notempty=0;
	}


	while(1)
	{

		//read descriptor
		descriptor =  set->get_array[thread_index].descriptor;
		put_count = set->put_array[thread_index].descriptor.put_count;

		// Read the global get window and possibly sync
		read_global_GWindow = global_GWindow.content;
		if (read_global_GWindow.max != thread_GWindow.max)
		{
			thread_GWindow = read_global_GWindow;
			hops = notempty = 0;
		}

		// Return valid descriptor if not empty sub-structure
		else if(descriptor.get_count < read_global_GWindow.max && (put_count != descriptor.get_count))
		{
			return descriptor;
		}

		// Hop
		else if (hops != set->width)
		{
			if (notempty == 0 && (put_count != descriptor.get_count))
			{
				notempty = 1;
			}

			thread_index = hop(set, thread_index, &random, &hops);
		}

		// Shift window
		else if (notempty)
		{
			if(thread_GWindow.max == global_GWindow.content.max)
			{
				new_window.max = thread_GWindow.max + set->depth;

				if(CAE(&global_GWindow.content, &thread_GWindow, &new_window))
				{
					my_slide_count+=1;
				}
			}

			thread_GWindow = global_GWindow.content;
			hops = notempty = 0;
		}

		// Empty return
		else
		{
			return descriptor;
		}

		
	}
}

uint64_t random_index(uint32_t width)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % width);
}

void initialize_global_window(uint64_t depth)
{
	/**
	 * Initializes the required global window variables
	 */

	global_PWindow.content.max = depth;
	global_GWindow.content.max = depth;
}
