/*
 * Author:  Kåre von Geijer <kare.kvg@gmail.com>
 * 			Adones <adones@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "multi-stack_random-relaxed.h"

RETRY_STATS_VARS;

#include "latency.h"

#ifdef RELAXATION_ANALYSIS
#include "relaxation_analysis_queue.c"
#endif

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

mstack_t* create_stack(size_t num_threads, uint64_t width, uint64_t relaxation_bound)
{
	mstack_t *set;

	if ((set = (mstack_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(mstack_t))) == NULL)
    {
		perror("malloc");
		exit(1);
    }
	set->array = (volatile index_t*)calloc(width, sizeof(index_t)); //ssalloc(width);
	set->width = width;

	int i;
	for(i=0; i < set->width; i++)
	{
		set->array[i].node = NULL;
	}

	return set;
}

int stack_cae(node_t** node_pointer_loc, node_t* read_node_pointer, node_t* new_node_pointer, int push)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();
	if (CAE(node_pointer_loc, &read_node_pointer, &new_node_pointer))
	{
		if (push) {
			new_node_pointer->val = gen_relaxation_count();
			add_linear(new_node_pointer->val, 1);
		}
		else
			remove_linear(read_node_pointer->val);

		unlock_relaxation_lists();
		return true;
	}
	else {
		unlock_relaxation_lists();
		return false;
	}

#else
	return CAE(node_pointer_loc, &read_node_pointer, &new_node_pointer);
#endif
}

int push(mstack_t *set, skey_t key, sval_t val)
{
	node_t *node, *new_node = create_node(key, val, NULL);
	uint64_t index;
	descriptor_t descriptor;
	while(1)
	{
		#if defined(NUM_CHOICES)
			descriptor = get_push_index(set);
			node = descriptor.node;
			index = descriptor.index;
		#else
			index = random_index(set);
			node = set->array[index].node;
		#endif
		new_node->next = node;
		new_node->timestamp = getticks();
		if(stack_cae(&set->array[index].node, node, new_node, 1))
		{
			return 1;
		}

		my_put_cas_fail_count+=1;
	}
}

sval_t pop(mstack_t *set)
{
	node_t *node, *next;
	descriptor_t descriptor;
	uint64_t index;
	while (1)
    {
		#if defined(NUM_CHOICES)
			descriptor = get_pop_index(set);
			node = descriptor.node;
			index = descriptor.index;
		#else
			index = random_index(set);
			node = set->array[index].node;
		#endif
		if(node != NULL)
		{
			next = node->next;
			if(stack_cae(&set->array[index].node, node, next, 0))
			{
				sval_t node_val = node->val;
				//garbage collector
				#if GC == 1
					ssmem_free(alloc, (void*) node);
				#endif
				return node_val;
			}
			my_get_cas_fail_count+=1;
		}
		else
		{
			my_null_count+=1;
			return 0;
		}
    }
}

#if defined(NUM_CHOICES)
	descriptor_t get_push_index(mstack_t *set)
	{
		int64_t i, index, index2;
		node_t *node, *node2;
		descriptor_t descriptor;

		index = random_index(set);
		node = set->array[index].node;
		if(node == NULL)
		{
			goto RETURN_NODE;
		}
		for(i=1;i < NUM_CHOICES;i++)
		{
			index2 = random_index(set);
			node2 = set->array[index2].node;
			if(node2 == NULL)
			{
				index = index2;
				node = node2;
				goto RETURN_NODE;
			}
			else if(node2->timestamp < node->timestamp)
			{
				index = index2;
				node = node2;
			}
		}
		RETURN_NODE:
		descriptor.node = node;
		descriptor.index = index;
		return descriptor;
	}
	descriptor_t get_pop_index(mstack_t *set)
	{
		int64_t i, s, index, index2;
		node_t *node, *node2;
		descriptor_t descriptor;

		index = random_index(set);
		RETRY:
		node = set->array[index].node;
		for(i=1; i < NUM_CHOICES; i++)
		{
			index2 = random_index(set);
			node2 = set->array[index2].node;
			if(node == NULL)
			{
				index = index2;
				node = node2;
			}
			else if(node2 == NULL)
			{
				continue;
			}
			else if(node2->timestamp > node->timestamp)
			{
				index = index2;
				node = node2;
			}
		}
		/* emptiness check
		if(node == NULL)
		{
			for(s=0; s < set->width; s++)
			{
				if(set->array[s].node != NULL)
				{
					index = s;
					goto RETRY;
				}
			}
		}
		*/
		descriptor.node = node;
		descriptor.index = index;
		return descriptor;
	}
#endif

uint64_t random_index(mstack_t *set)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (set->width));
}

size_t stack_size(mstack_t *set)
{
	size_t size = 0;
	uint64_t i;
	node_t *node;
	for(i=0; i < set->width; i++)
	{
		node=set->array[i].node;
		while (node != NULL)
		{
			size++;
			node = node->next;
		}
	}
	return size;
}