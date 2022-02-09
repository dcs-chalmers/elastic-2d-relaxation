#include "2Dc-window_elastic.h"
#include "lateral_stack.h"

#include "lateral_stack.c"


static width_t shift_width(lateral_stack_t* lateral, row_t bottom, width_t old_put_width, width_t new_put_width)
{
	row_t count;
	width_t max_width, current_width;
	lateral_node_t lat_node;
	lateral_descriptor_t lateral_descriptor = lateral->descriptor;

	max_width = new_put_width >= old_put_width ? new_put_width : old_put_width;

	// Iterate and take the largest width which is below bottom.
	lat_node.next_count = lateral_descriptor.count;
	lat_node.next = lateral_descriptor.node;

	// Bottom is the lowest possible descriptor row we can pop to, but we only care about widths above it
	while (unlikely(lat_node.next_count > bottom + 1))
	{
		lat_node = *lat_node.next;
		if (lat_node.width > max_width)
		{
			max_width = lat_node.width;
		}
	}

	return max_width;
}


static row_t put_shift_max(row_t old_max, depth_t new_depth)
{
	// Enforces that new max is higher than new depth
	depth_t shift;
	row_t new_max;

	shift = new_depth + 1 >> 1;

	new_max = old_max + shift;

	if (unlikely(new_max < new_depth)) {
		new_max = new_depth;
	}

	return new_max;

}


static row_t get_shift_max(row_t old_max, depth_t new_depth, depth_t old_depth)
{
	// Enforces that new max is higher than new depth
	depth_t shift = new_depth + 1 >> 1;

	if (likely(old_depth == new_depth))
	{
		if (likely(old_max >= shift + new_depth))
		{
			// No worry of underflowing or being smaller than new_depth
			return old_max - shift;
		}
	}
	else
	{
		if (old_max >= old_depth + shift)
		{
			// Also no danger of underflowing or being smaller than new_depth
			return old_max - old_depth - shift + new_depth;
		}
	}

	// Don't want to have a lower max than this.
	return new_depth;

}


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


static width_t sync_index(width_t width, width_t index)
{
	if (likely(index < width))
	{
		return index;
	}
	else
	{
		return 0;
	}
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
		thread_put_index = sync_index(thread_Window.put_width, thread_put_index);
	}

	if(contention || thread_put_index >= thread_Window.put_width)
	{
		thread_put_index = random_index(thread_Window.put_width);
	}

	while(1)
	{
		/* read descriptor */
		descriptor =  set->set_array[thread_put_index].descriptor;

		if (global_Window.content.version != thread_Window.version)
		{
			hops = 0;
			read_window();
			thread_put_index = sync_index(thread_Window.put_width, thread_put_index);
		}

		/* Try to work on the descriptor */
		else if(descriptor.count < thread_Window.max)
		{
			// Only sync if the get index is not outside the put width
			if (likely(thread_Window.put_width > thread_get_index))
			{
				thread_get_index = thread_put_index;
			}
			return descriptor;
		}

		/* hop */
		else if(hops != thread_Window.put_width)
		{
			thread_put_index = hop(set, thread_put_index, &random, &hops, thread_Window.put_width);
		}

		/* shift window */
		else
		{

			synchronize_lateral(set->lateral, set->set_array);

			new_window.old_put_width = thread_Window.put_width;

			new_window.put_width = set->width;
			new_window.version = thread_Window.version + 1;

			new_window.depth = set->depth;
			new_window.max = put_shift_max(thread_Window.max, new_window.depth);

			new_window.get_width = shift_width(set->lateral, new_window.max - new_window.depth,
											thread_Window.put_width, new_window.put_width);


			assert(new_window.max >= new_window.depth);

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
			thread_put_index = sync_index(thread_Window.put_width, thread_put_index);
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
		thread_get_index = sync_index(thread_Window.get_width, thread_get_index);
	}

	if(contention || thread_get_index >= thread_Window.get_width)
	{
		thread_get_index = random_index(thread_Window.get_width);
	}

	while(1)
	{

		/* read descriptor */
		descriptor =  set->set_array[thread_get_index].descriptor;

		/* Read the global window and possibly sync */
		if (global_Window.content.version != thread_Window.version)
		{
			hops = 0; empty = 1;
			read_window();
			thread_get_index = sync_index(thread_Window.get_width, thread_get_index);
		}

		/* empty sub-structures will be skipped at this point because (global_Window.content.max - set->depth) cannot go bellow zero */
		else if(descriptor.count > thread_Window.max - thread_Window.depth)
		{
			break;
		}

		/* change index (hop) */
		else if(hops != thread_Window.get_width)
		{
			/* emptiness check */
			if(descriptor.count > 0)
			{
				empty = 0;
			}
			thread_get_index = hop(set, thread_get_index, &random, &hops, thread_Window.get_width);
		}

		/* Return empty descriptor */
		else if (empty)
		{
			break;
		}

		/* shift window */
		else
		{

			synchronize_lateral(set->lateral, set->set_array);

			new_window.old_put_width = thread_Window.put_width;

			new_window.put_width = set->width;
			new_window.version = thread_Window.version + 1;

			new_window.depth = set->depth;
			new_window.max = get_shift_max(thread_Window.max, new_window.depth, thread_Window.depth);

			new_window.get_width = shift_width(set->lateral, new_window.max - new_window.depth,
											thread_Window.put_width, new_window.put_width);

			assert(new_window.max >= new_window.depth);

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

			thread_get_index = sync_index(thread_Window.get_width, thread_get_index);
			hops = 0; empty = 1;
		}
	}

	// Only sync if we can bring put index to get index
	if (likely(thread_Window.put_width > thread_get_index))
	{
		thread_put_index = thread_get_index;
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

	global_Window.content.depth = depth;

	global_Window.content.get_width = width;
	global_Window.content.put_width = width;
	global_Window.content.old_put_width = width;


}

