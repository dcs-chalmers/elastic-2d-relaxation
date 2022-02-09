#include "2Dc-window_optimized.h"


static inline uint64_t hop(DS_TYPE* set, uint64_t index, uint8_t* random, width_t* hops, width_t width)
{
	uint64_t old_index = index;

	my_hop_count += 1;

	if(*random < set->random_hops)
	{
		*random += 1;
		index = random_index(width);
	}
	else
	{
		*hops += 1;
		index += 1;

		if(index >= width)
		{
			index = 0;
		}
	}

	return index;
}


// Reads the global window into the thread local one, can think of it as atomic
static void read_window()
{
	// Opt: Can we read it in two consecutive parts? First in that case the version, and then the rest
	__atomic_load(&global_Window.content, &thread_Window, __ATOMIC_SEQ_CST);
}


descriptor_t put_window(DS_TYPE* set, uint8_t contention)
{
	window_t new_window;
	width_t hops;
	uint8_t random;
	descriptor_t descriptor;
	hops = random = 0;

	if(thread_Window.version != global_Window.content.version)
	{
		read_window();
	}

	if(contention)
	{
		thread_index = random_index(set->width);
	}

	while(1)
	{
		/* read descriptor */
		descriptor =  set->set_array[thread_index].descriptor;

		if (global_Window.content.version != thread_Window.version)
		{
			hops = 0;
			read_window();
		}

		/* Try to work on the descriptor */
		else if(descriptor.count < thread_Window.max)
		{
			return descriptor;
		}

		/* hop */
		else if(hops != set->width)
		{
			thread_index = hop(set, thread_index, &random, &hops, set->width);
		}

		/* shift window */
		else
		{

			new_window.version = thread_Window.version + 1;
			new_window.max = thread_Window.max + set->shift;

			// assert(new_window.max >= new_window.depth);

			if(thread_Window.version == global_Window.content.version)
			{
				if(CAE(&global_Window.content, &thread_Window, &new_window))
				{
					thread_Window = new_window;
					my_slide_count+=1;
				}
				else
				{
					read_window();
					my_slide_fail_count+=1;
				}
			}
			hops = 0;
		}
	}
}


descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	window_t new_window;
	width_t hops;
	uint8_t random; // shift
	hops = random = 0;
	descriptor_t descriptor;
	uint8_t empty = 1;

	if(thread_Window.version != global_Window.content.version)
	{
		read_window();
	}

	if(contention)
	{
		thread_index = random_index(set->width);
	}

	while(1)
	{

		/* read descriptor */
		descriptor =  set->set_array[thread_index].descriptor;

		/* Read the global window and possibly sync */
		if (global_Window.content.version != thread_Window.version)
		{
			hops = 0; empty = 1;
			read_window();
		}

		/* empty sub-structures will be skipped at this point because (global_Window.content.max - set->depth) cannot go bellow zero */
		else if(descriptor.count > thread_Window.max - set->depth)
		{
			break;
		}

		/* change index (hop) */
		else if(hops != set->width)
		{
			/* emptiness check */
			if(descriptor.count > 0)
			{
				empty = 0;
			}
			thread_index = hop(set, thread_index, &random, &hops, set->width);
		}

		/* Return empty descriptor */
		else if (empty)
		{
			break;
		}

		/* shift window */
		else
		{

			new_window.max = thread_Window.max - set->shift;
			new_window.version = thread_Window.version + 1;

			assert(new_window.max >= set->depth);

			// Should never want to shift down below zero right as we then would return empty instead?
			if(thread_Window.version == global_Window.content.version)
			{
				if(CAE(&global_Window.content, &thread_Window, &new_window))
				{
					thread_Window = new_window;
					my_slide_count+=1;
				}
				else
				{
					read_window();
					my_slide_fail_count+=1;
				}
			}

			hops = 0; empty = 1;
		}
	}

	return descriptor;
}


uint64_t random_index(width_t width)
{
	return my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % width;
}


void initialize_global_window(depth_t depth, width_t width)
{
	global_Window.content.max = depth;
	global_Window.content.version = 1;
}

