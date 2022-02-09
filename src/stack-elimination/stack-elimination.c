/*   
 *   File: stack-elimination.c
 *   Author: Adones <adones@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "stack-elimination.h"
#include "utils.h"

RETRY_STATS_VARS;

#include "latency.h"

#if LATENCY_PARSING == 1
	__thread size_t lat_parsing_get = 0;
	__thread size_t lat_parsing_put = 0;
	__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

extern __thread unsigned long* seeds;

sval_t mstack_elimination_find(mstack_t* qu, skey_t key)
{ 
	return 1;
}

int mstack_elimination_insert(mstack_t* qu, skey_t key, sval_t val)
{
	mstack_node_t* node = mstack_new_node(key, val, NULL);
	return mstack_elimination_push_node(qu,node);
}

/*ad*/
int mstack_elimination_push_node(mstack_t* qu, mstack_node_t* node)
{
	//NUM_RETRIES();
	while(1)
    {
		mstack_node_t* top = qu->top;
		node->next = top;
		if (CAS_PTR(&qu->top, top, node) == top) //ad reg,oldval,newval return oldval
		{
			return 1;
		}
		//elimination backoff
		else if (CAS_PTR(&elimination_array[thread_id].node, NULL, node) == NULL)
		{
			my_push_cas_fail_count+=1;
			//pause_rep(push_wait);//DO_PAUSE();
			for(wait_array[thread_id].count=0;wait_array[thread_id].count<push_wait;wait_array[thread_id].count++)
			{
				_mm_pause();
			}
			if(node!=NULL&&CAS_PTR(&elimination_array[thread_id].node, node, NULL) == node);
			else return 1;			
		}
    }
}

sval_t mstack_elimination_delete(mstack_t* qu)
{
	int i=0;
	mstack_node_t* top;
	//NUM_RETRIES();
	while (1)
    {
		top = qu->top;
		//if stack is empty
		if (unlikely(top == NULL))
		{
			my_null_count+=1;
			return 0;
		}
		else if (CAS_PTR(&qu->top, top, top->next) == top)
		{
			return mstack_elimination_delete_node(top);
		}
		//back off
		else
		{
			//my_pop_cas_fail_count+=1;
			while(i<num_threads)
			{
				pop_slot = random_slot();
				if(pop_slot!=thread_id)
				{
					mstack_node_t* node = elimination_array[pop_slot].node;
					if (node!=NULL&&CAS_PTR(&elimination_array[pop_slot].node, node, NULL) == node)
					{	
						my_pop_cas_fail_count+=1;
						//wait_array[pop_slot].count = 0;// push_wait; stop push thread from waiting
						return mstack_elimination_delete_node(node);
					}
				}
				i++;
			}
		}
		//pause_rep(pop_wait);//DO_PAUSE();
		for(wait_array[thread_id].count=0;wait_array[thread_id].count<pop_wait;wait_array[thread_id].count++)
		{
			_mm_pause();
		}
    }
}
sval_t mstack_elimination_delete_node(mstack_node_t* node)
{
	sval_t node_val = node->val;
	//garbage collector
	#if GC == 1
		ssmem_free(alloc, (void*) node);
	#endif
	return node_val;
}
unsigned long random_slot()
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (num_threads));
}