/*   
 *   File: ms.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description:  
 *   ms.c is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "multi-queue_2dd-window.h"
#include "relaxation_2dd-window.c"

RETRY_STATS_VARS;

#include "latency.h"
#if LATENCY_PARSING == 1
__thread size_t lat_parsing_get = 0;
__thread size_t lat_parsing_put = 0;
__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

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

mqueue_t* create_queue(size_t num_threads, uint64_t width, uint64_t depth, uint8_t k_mode, uint64_t relaxation_bound)
{
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

	set->get_array = (volatile index_t*)calloc(width, sizeof(index_t)); //ssalloc(width); 
	set->put_array = (volatile index_t*)calloc(width, sizeof(index_t)); //ssalloc(width); 
	set->width = width;
	set->depth = depth;
	set->random_hops = 2;
	set->k_mode = k_mode;
	set->relaxation_bound = relaxation_bound;
	
	uint64_t i;
	node_t *node;
	for(i=0; i < set->width; i++)
	{
		node = (node_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(node_t));
		node->next = NULL;
		set->put_array[i].descriptor.node = set->get_array[i].descriptor.node = node;
		set->put_array[i].descriptor.put_count = 0;
		set->get_array[i].descriptor.get_count = 0;
	}

	return set;
}

int enqueue(mqueue_t* set, skey_t key, sval_t val)
{
	node_t* tail;
	uint8_t contetion = 0;
	descriptor_t descriptor, new_descriptor;
	
	node_t* new_node = create_node(key, val, NULL);	
	while(1)
    {		
		descriptor = put_window(set,contetion);	
		tail = descriptor.node;
		new_descriptor.node = new_node;
		new_descriptor.put_count = descriptor.put_count + 1;	
		if(tail->next == NULL)
		{
			if(CAS(&tail->next, NULL, new_node))
			{
				break;
			}
			else
			{
				contetion = 1;
			}
		}
		else
		{
			//Try helping pending enqueue
			new_descriptor.node = tail->next;
			if(!CAE(&set->put_array[thread_index].descriptor,&descriptor,&new_descriptor))
			{
				contetion = 1;
			}
		}
		my_put_cas_fail_count+=1;
    }
	if(!CAE(&set->put_array[thread_index].descriptor,&descriptor,&new_descriptor));
	{
		contetion = 1;
	}
	return 1;
}

sval_t dequeue(mqueue_t* set)
{
	sval_t val;
	node_t *head, *tail;
	uint8_t contetion = 0;
	descriptor_t enq_descriptor, new_enq_descriptor, deq_descriptor, new_deq_descriptor;

	while (1)
    {
		deq_descriptor = get_window(set,contetion);			
		head = deq_descriptor.node;
		enq_descriptor = set->put_array[thread_index].descriptor;
		tail = enq_descriptor.node;
			
		if (head == tail)
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
					contetion = 1;
				}	
			}
		}
		else
		{
			new_deq_descriptor.node = head->next;
			new_deq_descriptor.get_count = deq_descriptor.get_count + 1;			
			if(CAE(&set->get_array[thread_index].descriptor,&deq_descriptor,&new_deq_descriptor))
			{
				val = head->next->val;
				#if GC == 1
					ssmem_free(alloc, (void*) head);
				#endif
				return val;
			}
			else
			{								
				contetion = 1;
			}
		}
		my_get_cas_fail_count+=1;
    }
}
size_t queue_size(mqueue_t *set)
{
	size_t size = 0;
	uint64_t q;
	node_t *head, *tail;
	for(q=0; q < set->width; q++)
	{
		head = set->get_array[q].descriptor.node;
		tail = set->put_array[q].descriptor.node;
		//size+= (set->put_array[q].descriptor.put_count - set->get_array[q].descriptor.get_count);
		while (head!=tail)
		{					
			head = head->next;
			size+=1;		
		}
	}
	return size;
}
