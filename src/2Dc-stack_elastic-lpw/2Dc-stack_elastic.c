/*
 * Author: Kåre von Geijer <karev@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "2Dc-stack_elastic.h"
#include "lateral_stack.h"
#include "2Dc-window_elastic.c"


#ifdef RELAXATION_ANALYSIS
#include "relaxation_analysis_queue.c"
#endif

RETRY_STATS_VARS;

#include "latency.h"

#if LATENCY_PARSING == 1
	__thread size_t lat_parsing_get = 0;
	__thread size_t lat_parsing_put = 0;
	__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

extern __thread unsigned long* seeds;

node_t* create_node(skey_t key, sval_t val, node_t* next)
{
	#if GC == 1
		node_t *node = ssmem_alloc(alloc, sizeof(node_t));
	#else
	  	node_t* node = ssalloc(sizeof(node_t));
	#endif
	node->key = key;
	node->val = val;
	node->next = next;

	#ifdef __tile__
		MEM_BARRIER;
	#endif

	return node;
}

mstack_t* create_stack(size_t num_threads, width_t width, depth_t depth, width_t max_width, uint8_t k_mode, uint64_t relaxation_bound)
{
	mstack_t *set;

	/****
		calculate width and depth using the relaxation bound (K = (2*shift+depth)(width−1))
		We use shift = depth/2 which gives K = (2depth)(width−1)
	****/
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
			depth = relaxation_bound / (2*(width - 1));
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / (2*depth)) + 1;
			}
		}
	}
	else if(k_mode == 2)
	{
		//maximum depth is fixed
		width = (relaxation_bound / (2*depth)) + 1;
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
			depth = relaxation_bound / (2*(width - 1));
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / (2*depth)) + 1;
			}
		}
	}
	else if(k_mode == 0)
	{
		relaxation_bound = 2 * depth * (width -1);
	}
	/*************************************************************/

	initialize_global_window(depth, width);

	if (max_width < width)
	{
		max_width = width;
	}

	if ((set = (mstack_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(mstack_t))) == NULL)
    {
		perror("malloc");
		exit(1);
    }
	set->set_array = (volatile index_t*) ssalloc_aligned(CACHE_LINE_SIZE, max_width*sizeof(index_t));
	set->lateral = create_lateral_stack(max_width);
	set->width = width;
	set->max_width = max_width;
	set->depth = depth;
	set->random_hops = 2;
	set->k_mode = k_mode;
	set->relaxation_bound = relaxation_bound;

	int i;
	for(i=0; i < set->max_width; i++)
	{
		set->set_array[i].descriptor.node = NULL;
		set->set_array[i].descriptor.count = 0;
	}
	return set;
}

int stack_cae(descriptor_t* des_loc, descriptor_t* read_des_loc, descriptor_t* new_des_loc, int push)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();
	if (CAE(des_loc, read_des_loc, new_des_loc))
	{
		if (push) {
			new_des_loc->node->val = gen_relaxation_count();
			add_linear(new_des_loc->node->val, 1);
		}
		else
			remove_linear(read_des_loc->node->val);

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

int push(mstack_t *set, skey_t key, sval_t val)
{
	uint8_t contention = 0;
	descriptor_t descriptor, new_descriptor;

	node_t* new_node = create_node(key, val, NULL);
	while(1)
	{
		descriptor = put_window(set, contention);

		new_node->next = descriptor.node;
		new_node->next_count = descriptor.count;

		new_descriptor.node = new_node;

		// Don't allow pushes below the window.
		if (likely(descriptor.count + thread_Window.depth >= thread_Window.max))
		{
			new_descriptor.count = descriptor.count + 1;
		}
		else {
			new_descriptor.count = thread_Window.max - thread_Window.depth;
		}


		if(stack_cae(&set->set_array[thread_put_index].descriptor, &descriptor, &new_descriptor, 1))
		{
			return 1;
		}
		else
		{
			contention = 1;
		}

		my_put_cas_fail_count += 1;
	}
}

sval_t pop(mstack_t *set)
{
	uint8_t contention = 0;
	descriptor_t descriptor, new_descriptor;

	while (1)
    {
		descriptor = get_window(set, contention);
		if(descriptor.node != NULL)
		{
			new_descriptor.node = descriptor.node->next;
			new_descriptor.count = descriptor.node->next_count;

			if(stack_cae(&set->set_array[thread_get_index].descriptor, &descriptor, &new_descriptor, 0))
			{
				sval_t node_val = descriptor.node->val;
				//garbage collector
				#if GC == 1
					ssmem_free(alloc, (void*) descriptor.node);
				#endif
				return node_val;
			}
			else
			{
				contention = 1;
			}

			my_get_cas_fail_count += 1;
		}
		else
		{
			my_null_count += 1;
			return 0;
		}
    }
}

size_t stack_size(mstack_t *set)
{
	size_t size = 0;
	uint64_t i;
	node_t *node;
	for(i=0; i < set->max_width; i++)
	{
		node = set->set_array[i].descriptor.node;
		while (node != NULL)
		{
			size++;
			node = node->next;
		}
	}
	return size;
}


depth_t update_depth(mstack_t *set, depth_t depth)
{
	/* Changes the depth of the future global windows */

	depth_t old_depth = set->depth;
	while (!CAE(&set->depth, &old_depth, &depth))
		{
			// Keep trying until success
		}

	return old_depth;

}

width_t update_width(mstack_t *set, width_t width)
{
	/* Changes the active width of the future global windows */

	width_t old_width = set->width;
	assert(width <= set->max_width);

	while (!CAE(&set->width, &old_width, &width))
	{
		// Keep trying until success
	}

	return old_width;

}
