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

#include "queue-q_relaxed-k-segment.h"
#include "utils.h"
#if defined(RELAXATION_ANALYSIS)
	#include "relaxation_analysis_queue.c"
#endif

RETRY_STATS_VARS;

#include "latency.h"
extern __thread unsigned long* seeds;

sval_t queue_relaxed_find(queue_t* qu, skey_t key)
{
	return 1;
}

static int enq_cae(queue_node_t** next_loc, queue_node_t* new_node)
{
	queue_node_t* expected = NULL;
#ifdef RELAXATION_ANALYSIS

	lock_relaxation_lists();

	if (CAE(next_loc, &expected, &new_node))
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
	return CAE(next_loc, &expected, &new_node);
#endif
}

///ad
int queue_relaxed_insert(skey_t key, sval_t val)
{
	queue_node_t* new_node = queue_new_node(key, val, NULL);

	while(1)
	{
		get_push_slot();
		//if CAS fails retry on same slot
		if(enq_cae(&current_segment->indices[push_slot].node, new_node))
		{
			return 1;
		}
		my_push_cas_fail_count+=1;
	}
}
void get_push_slot()
{
	push_slot = random_slot();
	current_segment=tail;
	walk_count = 0;
	while(1)
	{
		if(push_slot>=segment_size) push_slot=0;
		current_node=current_segment->indices[push_slot].node;
		//if valid slot is found break out of the loop
		if(current_node==NULL)
		{
			break;
		}
		else if(current_segment==tail)
		{
			push_slot+=1;
			my_hop_count+=1;
		}
		else
		{
			current_segment = tail;
			walk_count = 0;
		}
		if(walk_count>=segment_size)
		{
			try_create_segment();
			current_segment = tail;
			walk_count=0;
		}
		else
			walk_count+=1;
	}
}
sval_t queue_relaxed_delete()
{
	sval_t node_val;
	while (1)
    {
		get_pop_slot();
		if(deleted_node==1||current_node==NULL)
		{
			my_null_count+=1;
			return 0;
		}
		#if defined(RELAXATION_ANALYSIS)
		lock_relaxation_lists();
		#endif
		if(CAS(&current_segment->indices[pop_slot].deleted, 0, 1)) {
			node_val = current_node->val;
			#ifdef RELAXATION_ANALYSIS
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
	current_segment = head; //get segment to work on and store it locally
	walk_count=0;
	volatile int pending_slot=0;
	pop_slot = random_slot();
	while(1)
	{
		if(walk_count>=segment_size)
		{
			if(current_segment==tail)//if last segment
			{
				break;
			}
			else if(pending_slot==0)
			{
				if(CAS(&head, current_segment, current_segment->next))
				{
					#if GC == 1
						ssmem_free(alloc_segment, (void*) current_segment);
						ssmem_free(alloc_segment, (void*) current_segment->indices);
					#endif
				}
				current_segment = head;
				walk_count=0;
			}
		}
		else
			walk_count+=1;

		if(pop_slot>=segment_size) pop_slot=0;
		current_node=current_segment->indices[pop_slot].node;
		deleted_node=current_segment->indices[pop_slot].deleted;
		//if valid slot is found break out of the loop
		if(current_node!=NULL && deleted_node==0)
		{
			break;
		}
		else if(current_segment==head)//check if top has not changed since the index search started
		{
			pop_slot+=1;
			my_hop_count+=1;
			if(current_node == NULL) pending_slot=1;
		}
		else
		{
			current_segment=head;
			walk_count = 0;
		}
	}
}
void try_create_segment()
{
	//create segment and move pointer to the new segment. If it fails release memory of new segment.
	segment_t* new_segment;
	index_t* indices_memory;

	//indices_memory = calloc(segment_size, sizeof(index_t));
	//new_segment = calloc(1, sizeof(segment_t));
	indices_memory = ssmem_alloc(alloc_segment, sizeof(index_t)*segment_size);
	new_segment = ssmem_alloc(alloc_segment, sizeof(segment_t));

	new_segment->next = NULL;
	new_segment->indices = indices_memory;
	assert(indices_memory != NULL);

	if(CAS(&current_segment->next, NULL, new_segment))
	{
		if(CAS(&tail, current_segment, new_segment))
			my_slide_count+=1;
	}
	else if(current_segment->next != NULL && tail==current_segment)
	{
		//help move poiter if there is a pending segement
		CAS(&tail,current_segment,current_segment->next);
		#if GC == 1
			ssmem_free(alloc_segment, (void*) new_segment);
			ssmem_free(alloc_segment, (void*) indices_memory);
		#endif
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
int queue_size(queue_t *set)
{
	int size = 0;
	int i;
	queue_node_t *node;
	segment_t* current_segment = head;
	while(1)
	{
		for(i=0; i < segment_size; i++)
		{
			if(current_segment->indices[i].node != NULL && current_segment->indices[i].deleted == 0) size++;
		}
		if(current_segment==tail) break;
		else current_segment=current_segment->next;
	}
	return size;
}