#include "2Dc-window.h"


static inline uint64_t hop(DS_TYPE* set, uint64_t index, uint64_t* random, uint64_t* hops)
{
	uint64_t old_index = index;

	my_hop_count += 1;

	if(*random < set->random_hops)
	{
		*random += 1;
		index = random_index(set->width);
	}
	else
	{
		*hops += 1;
		index += 1;

		if(index >= set->width)
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
	window_t read_global_Window, new_window;
	uint64_t hops, random;
	descriptor_t descriptor;
	hops = random = 0;

	if(thread_Window.version != global_Window.content.version)
	{
		read_window();
	}

	if(contention == 1)
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
			thread_index = hop(set, thread_index, &random, &hops);
		}

		/* shift window */
		else
		{

			new_window.max = thread_Window.max + set->shift;
			new_window.version = thread_Window.version + 1;

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
	window_t read_global_Window, new_window;
	uint16_t hops, random, shift;
	hops = random = 0;
	descriptor_t descriptor;
	uint8_t empty = 1;

	if(thread_Window.version != global_Window.content.version)
	{
		read_window();
	}

	if(contention == 1)
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
		else if(descriptor.count + set->depth > thread_Window.max)
		{
			return descriptor;
		}

		/* change index (hop) */
		else if(hops != set->width)
		{
			/* emptiness check */
			if(descriptor.count > 0)
			{
				empty = 0;
			}
			thread_index = hop(set, thread_index, &random, &hops);
		}

		/* Return empty descriptor */
		else if (empty)
		{
			return descriptor;
		}

		/* shift window */
		else
		{


			new_window.version = thread_Window.version + 1;
			new_window.max = thread_Window.max - set->shift;

			/* maintains (global_Window.content.max >= global_Window.content.depth) */
			if(thread_Window.version == global_Window.content.version && new_window.max >= set->depth)
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
}


uint64_t random_index(uint16_t width)
{
	return my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % width;
}


void initialize_global_window(uint16_t depth)
{
	global_Window.content.max = depth;
	global_Window.content.version = 1;

}
