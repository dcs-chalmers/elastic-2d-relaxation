/*
 *
 *   Author: Adones <adones@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "stack-s_relaxed-k-segment.h"
#include "utils.h"
//ad
#if defined(RELAXATION_ANALYSIS)
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

sval_t mstack_relaxed_find(mstack_t* qu, skey_t key)
{
	return 1;
}

int mstack_relaxed_insert(skey_t key, sval_t val)
{
	mstack_node_t* new_node = mstack_new_node(key, val, NULL);
	while(1)
	{
		get_push_slot();

		#if defined(RELAXATION_ANALYSIS)
		lock_relaxation_lists();
		#endif

		if(CAS(&current_segment->indices[push_slot].node, NULL, new_node))
		{
			if(check_commit(new_node))
			{
				#if defined(RELAXATION_ANALYSIS)
				val = gen_relaxation_count();
				new_node->val = val;
				add_linear(val, 1);
				unlock_relaxation_lists();
				#endif
				return 1;
			}
		}
		#if defined(RELAXATION_ANALYSIS)
		unlock_relaxation_lists();
		#endif
		my_push_cas_fail_count+=1;
	}
}
void get_push_slot()
{
	push_slot = random_slot();
	current_head = *head;
	current_segment=current_head.segment;
	walk_count = 0;
	while(1)
	{
		if(walk_count>=segment_size)
		{
			try_create_segment();
			current_head = *head;
			current_segment = current_head.segment;
			walk_count=1;
		}
		else
			walk_count+=1;

		if(push_slot>=segment_size) push_slot=0;
		current_node=current_segment->indices[push_slot].node;
		//if valid slot is found break out of the loop
		if(current_node==NULL)
		{
			break;
		}
		else if(current_segment==head->segment)
		{
			push_slot+=1;
			my_hop_count+=1;
		}
		else
		{
			current_head = *head;
			current_segment = current_head.segment;
			walk_count = 0;
		}
	}
}
int check_commit(mstack_node_t* new_node)
{
	head_t new_head;
	if (current_segment->indices[push_slot].node != new_node)
	{
		// Been dequeued, so must have been enqueued
		return 1;
	}
	if (current_segment->remove == 0)
	{
		return 1;
	}
	/*if (current_segment->remove > 0)
	{
		if (current_segment != head->segment) //if head->segment has changed
		{
			if (!CAS(&current_segment->indices[push_slot].node, new_node, NULL))
			{
				return 1;
			}
		}
		else
		{
			new_head.segment = current_segment;
			new_head.version = current_head.version+1;
			if(CAE(head, &current_head, &new_head))//try to change head version to stop remove segment
			{
				return 1;
			}
			if(!CAS(&current_segment->indices[push_slot].node, new_node, NULL))
			{
				return 1;
			}
		}
	}*/
	if (!CAS(&current_segment->indices[push_slot].node, new_node, NULL))
	{
		return 1;
	}
	return 0;
}
sval_t mstack_relaxed_delete()
{
	sval_t node_val;
	while (1)
    {
		get_pop_slot();
		if(current_node==NULL)
		{
			my_null_count+=1;
			return 0;
		}
		#if defined(RELAXATION_ANALYSIS)
		lock_relaxation_lists();
		#endif
		if(CAS(&current_segment->indices[pop_slot].node, current_node, NULL))
		{
			node_val = current_node->val;
			#if defined(RELAXATION_ANALYSIS)
				remove_linear(node_val);
				unlock_relaxation_lists();
			#endif
			#if GC == 1
				ssmem_free(alloc, (void*) current_node);
			#endif
			return node_val;
		}
		#if defined(RELAXATION_ANALYSIS)
			unlock_relaxation_lists();
		#endif
		my_pop_cas_fail_count+=1;
    }
}
void get_pop_slot()
{
	walk_count=0;
	pop_slot = random_slot();
	int marked=0;
	int deleted;
	head_t new_head;
	current_head = *head;
	current_segment = current_head.segment;//get segment to work on and store it locally
	while(1)
	{
		deleted=0;
		if(walk_count==segment_size&&!current_segment->next==NULL)//if not last segement mark the head to indicate possiblity of remove
		{
			IAF_U64(&current_segment->remove);
			marked=1;
		}
		if(walk_count>=(segment_size*2))//do two passes over the segment
		{
			if(current_segment->next==NULL)//if last segment
			{
				#if EXACT_EMPTINESS==1
				if(current_head.version==head->version)
				{
					break;
				}
				#else
				if(current_segment==head->segment)
				{
					break;
				}
				#endif
			}
			else
			{
				//try remove segment
				new_head.segment = current_segment->next;
				new_head.version = current_head.version+1;
				if(CAE(head, &current_head, &new_head))
				{
					deleted=1;
					#if GC == 1
						ssmem_free(alloc_segment, (void*) current_segment);
						ssmem_free(alloc_segment, (void*) current_segment->indices);
					#endif
					my_slide_count+=1;
				}
			}
			if(deleted==0)
			{
				DAF_U64(&current_segment->remove);
			}
			marked=0;
			current_head = *head;
			current_segment = current_head.segment;
			walk_count=1;
		}
		else
			walk_count+=1;

		if(pop_slot>=segment_size) pop_slot=0;
		current_node=current_segment->indices[pop_slot].node;
		//if valid slot is found break out of the loop
		if(current_node!=NULL)
		{
			if(marked==1)DAF_U64(&current_segment->remove);
			break;
		}
		else if(current_segment==head->segment)//check if top has not changed since the index search started
		{
			pop_slot+=1;
			my_hop_count+=1;
		}
		else
		{
			if(marked==1)
			{
				DAF_U64(&current_segment->remove);
				marked=0;
			}
			current_head = *head;
			current_segment = current_head.segment;
			walk_count = 0;
		}
	}
}
void try_create_segment()
{
	//create segment and move pointer to the new segment. If it fails release memory of new segment.
	segment_t* new_segment;
	index_t* indices_memory;
	head_t new_head;

	//indices_memory = calloc(segment_size, sizeof(index_t));
	//new_segment = calloc(1, sizeof(segment_t));
	indices_memory = ssmem_alloc(alloc_segment, sizeof(index_t)*segment_size);
	new_segment = ssmem_alloc(alloc_segment, sizeof(segment_t));

	new_segment->next = current_segment;
	new_segment->remove = 0;
	new_segment->indices = indices_memory;
	assert(indices_memory != NULL);

	new_head.segment = new_segment;
	new_head.version = current_head.version+1;

	if(CAE(head, &current_head, &new_head))
	{
		my_slide_count+=1;
	}
	else
	{
		#if GC == 1
			ssmem_free(alloc_segment, (void*) new_segment);
			ssmem_free(alloc_segment, (void*) indices_memory);
		#endif
	}
}

unsigned long random_slot()
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (segment_size));
}
int mstack_size(mstack_t *set)
{
	int size = 0;
	int i;
	mstack_node_t *node;
	segment_t* current_segment = head->segment;
	while(1)
	{
		for(i=0; i < segment_size; i++)
		{
			if(current_segment->indices[i].node != NULL) size++;
		}
		if(current_segment->next==NULL) break;
		else current_segment=current_segment->next;
	}
	return size;
}