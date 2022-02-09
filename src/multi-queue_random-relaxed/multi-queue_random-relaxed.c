/*
 * Author: Adones <adones@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Discritors are synchronised globally (global descriptors )before each operation is curried out
 */

#include "multi-queue_random-relaxed.h"

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

mqueue_t* create_queue(size_t num_threads, uint64_t width, uint64_t relaxation_bound)
{
	mqueue_t *set;

	if ((set = (mqueue_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(mqueue_t))) == NULL)
    {
		perror("malloc");
		exit(1);
    }
	set->get_array = ssalloc_aligned(CACHE_LINE_SIZE, width*sizeof(index_t)); //ssalloc(width);
	set->put_array = ssalloc_aligned(CACHE_LINE_SIZE, width*sizeof(index_t)); //ssalloc(width);
	set->width = width;


	uint64_t i;
	node_t *node;
	for(i=0; i < set->width; i++)
	{
		node = (node_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(node_t));
		node->next = NULL;
		set->put_array[i].node = set->get_array[i].node = node;
	}

	return set;
}

static int enq_cae(node_t** next_node_loc, node_t* new_node)
{
	node_t* expected = NULL;
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();

	if (CAE(next_node_loc, &expected, &new_node))
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
	return CAE(next_node_loc, &expected, &new_node);
#endif
}

static int deq_cae(node_t** node_pointer_loc, node_t* read_node_pointer, node_t* new_node_pointer)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();
	if (CAE(node_pointer_loc, &read_node_pointer, &new_node_pointer))
	{
		remove_linear(new_node_pointer->val);
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

int enqueue(mqueue_t *set, skey_t key, sval_t val)
{
	node_t *tail, *new_node = create_node(key, val, NULL);
	uint64_t index;
	descriptor_t descriptor;
	while(1)
	{
		#if defined(NUM_CHOICES)
			descriptor = get_enqueue_index(set);
			tail = descriptor.node;
			index = descriptor.index;
		#else
			index = random_index(set);
			tail = set->put_array[index].node;
		#endif
		new_node->timestamp = getticks();
		if(tail->next == NULL)
		{
			if(enq_cae(&tail->next, new_node))
			{
				break;
			}
		}
		else
		{
			CAS(&set->put_array[index].node, tail, tail->next);
		}

		my_put_cas_fail_count+=1;
	}
	CAS(&set->put_array[index].node,tail,new_node);

	return 1;
}

sval_t dequeue(mqueue_t *set)
{
	node_t *head, *tail, *next;
	descriptor_t descriptor;
	uint64_t index;
	sval_t val;
	while (1)
    {
		#if defined(NUM_CHOICES)
			descriptor = get_dequeue_index(set);
			head = descriptor.node;
			index = descriptor.index;
		#else
			index = random_index(set);
			head = set->get_array[index].node;
		#endif
		tail = set->put_array[index].node;

		if (unlikely(head == tail))
		{
			if(head->next == NULL)
			{
				my_null_count+=1;
				return 0;
			}
			else
			{
				CAS(&set->put_array[index].node,tail,tail->next);
			}
		}
		else
		{
			if(deq_cae(&set->get_array[index].node, head, head->next))
			{
				val = head->next->val;
				#if GC == 1
					ssmem_free(alloc, (void*) head);
				#endif
				return val;
			}
		}
    }
}

#if defined(NUM_CHOICES)
	descriptor_t get_enqueue_index(mqueue_t *set)
	{
		int64_t i, index, index2;
		node_t *node, *node2;
		descriptor_t descriptor;

		index = random_index(set);
		node = set->put_array[index].node;
		if(node == NULL)
		{
			goto RETURN_NODE;
		}
		for(i=1;i < NUM_CHOICES;i++)
		{
			index2 = random_index(set);
			node2 = set->put_array[index2].node;
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
	descriptor_t get_dequeue_index(mqueue_t *set)
	{
		int64_t i, s, index, index2;
		node_t *node, *node2;
		descriptor_t descriptor;

		index = random_index(set);
		RETRY:
		node = set->get_array[index].node;
		for(i=1; i < NUM_CHOICES; i++)
		{
			index2 = random_index(set);
			node2 = set->get_array[index2].node;
			if(node->next == NULL)
			{
				index = index2;
				node = node2;
			}
			else if(node2->next == NULL)
			{
				continue;
			}
			else if(node2->timestamp < node->timestamp)
			{
				index = index2;
				node = node2;
			}
		}
		/* emptiness check
		if(node->next == NULL)
		{
			for(s=0; s < set->width; s++)
			{
				if(set->get_array[s].node->next != NULL)
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

uint64_t random_index(mqueue_t *set)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (set->width));
}

size_t queue_size(mqueue_t *set)
{
	size_t size = 0;
	uint64_t i;
	node_t *node;
	for(i=0; i < set->width; i++)
	{
		node=set->get_array[i].node;
		while (node->next != NULL)
		{
			size++;
			node = node->next;
		}
	}
	return size;
}