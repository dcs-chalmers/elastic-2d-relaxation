#include "2Dd-queue.h"
#include "2Dd-window.c"

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

void free_node(node_t* node)
{
	#if GC == 1
		ssmem_free(alloc, (void*) node);
	#endif
}

node_t* create_node(skey_t key, sval_t val, node_t* next)
{
	#if GC == 1
	  	node_t* node = ssmem_alloc(alloc, sizeof(node_t));
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

mqueue_t* create_queue(size_t num_threads, uint32_t width, uint64_t depth, uint8_t k_mode, uint64_t relaxation_bound)
{
	// Creates the data structure, including windows

	mqueue_t *set;

	/**** calculate width and depth using the relaxation bound ****/
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
			depth = relaxation_bound / (width - 1);
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / depth) + 1;
			}
		}
	}
	else if(k_mode == 2)
	{
		//maximum depth is fixed
		width = (relaxation_bound / depth) + 1;
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
			depth = relaxation_bound / (width - 1);
			if(depth<1)
			{
				depth = 1;
				width = (relaxation_bound / depth) + 1;
			}
		}
	}
	else if(k_mode == 0)
	{
		relaxation_bound = depth * (width -1);
	}
	/*************************************************************/

	if ((set = (mqueue_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(mqueue_t))) == NULL)
	{
	  perror("malloc");
	  exit(1);
	}


	// Initialize all descriptors to zero (empty)
	set->get_array = (volatile index_t*)ssalloc_aligned(CACHE_LINE_SIZE, width*sizeof(index_t)); //ssalloc(width);
	set->put_array = (volatile index_t*)ssalloc_aligned(CACHE_LINE_SIZE, width*sizeof(index_t));//ssalloc(width);
	set->random_hops = 2;
	set->depth = depth;
	set->width = width;
	set->k_mode = k_mode;
	set->relaxation_bound = relaxation_bound;

	// Initlialize the window variables
	initialize_global_window(depth);

	uint64_t i;
	node_t *node;
	for(i = 0; i < width; i++)
	{
		//node = create_node(0, 0, NULL);	// This thread isn't properly initialized
		node = (node_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(node_t));
		if (node == NULL)
			printf("ERROR: Memory ran out when allocating queue");
		node->next = NULL;

		set->put_array[i].descriptor.node = set->get_array[i].descriptor.node = node;
		set->put_array[i].descriptor.put_count = 0;
		set->get_array[i].descriptor.get_count = 0;
	}

	return set;
}


static int enq_cas(node_t** next_loc, node_t* new_node)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();

	if (CAS(next_loc, NULL, new_node))
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
		return CAS(next_loc, NULL, new_node);
#endif
}

static int deq_cae(descriptor_t* des_loc, descriptor_t* read_des_loc, descriptor_t* new_des_loc)
{
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();
	if (CAE(des_loc, read_des_loc, new_des_loc))
	{
		remove_linear(new_des_loc->node->val);
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


int enqueue(mqueue_t* set, skey_t key, sval_t val)
{
	node_t* tail;
	uint8_t contention = 0;
	descriptor_t descriptor, new_descriptor;

	node_t* new_node = create_node(key, val, NULL);
	while(1)
    {
		descriptor = put_window(set,contention);

		tail = descriptor.node;

		new_descriptor.node = new_node;
		new_descriptor.put_count = descriptor.put_count + 1;

		if(tail->next == NULL)
		{
			if (set->put_array[thread_index].descriptor.put_count >= thread_PWindow.max) {
				// Double check needed as the descriptor is not read atomically, and the node is written without ensuring it is read correctly
				continue;
			}

			if(enq_cas(&tail->next, new_node))
			{
				break;
			}
			else
			{
				contention = 1;
			}
		}
		else
		{
			//Try helping pending enqueue
			new_descriptor.node = tail->next;
			if(!CAE(&set->put_array[thread_index].descriptor,&descriptor,&new_descriptor))
			{
				contention = 1;
			}
		}
		my_put_cas_fail_count+=1;
    }
	if(!CAE(&set->put_array[thread_index].descriptor,&descriptor,&new_descriptor));
	{
		contention = 1;
	}
	return 1;
}

sval_t dequeue(mqueue_t* set)
{
	sval_t val;
	node_t *head, *tail;
	uint8_t contention = 0;
	descriptor_t enq_descriptor, new_enq_descriptor, deq_descriptor, new_deq_descriptor;

	while (1)
    {
		deq_descriptor = get_window(set,contention);
		head = deq_descriptor.node;
		enq_descriptor = set->put_array[thread_index].descriptor;
		tail = enq_descriptor.node;

		if (unlikely(head == tail))
		{
			if(head->next == NULL)
			{
				my_null_count+=1;
				return 0;
			}
			else
			{
				//Try helping pending enqueue
				new_enq_descriptor.node = tail->next;
				new_enq_descriptor.put_count = enq_descriptor.put_count + 1;
				if(!CAE(&set->put_array[thread_index].descriptor,&enq_descriptor,&new_enq_descriptor))
				{
					contention = 1;
				}
			}
		}
		else
		{
			new_deq_descriptor.node = head->next;
			new_deq_descriptor.get_count = deq_descriptor.get_count + 1;
			if(deq_cae(&set->get_array[thread_index].descriptor, &deq_descriptor, &new_deq_descriptor))
			{

				val = head->next->val;
				free_node(head);
				return val;

			}
			else
			{
				contention = 1;
			}
		}
		my_get_cas_fail_count+=1;
    }
}
size_t queue_size(mqueue_t *set)
{
	size_t size = 0;
	node_t *head, *tail;


	for(int q = 0; q < set->width; q++)
	{
		head = set->get_array[q].descriptor.node;
		tail = set->put_array[q].descriptor.node;

		while (head != tail)
		{
			head = head->next;
			size+=1;
		}
	}

	return size;
}
