#include <stdio.h>
#include <stdlib.h>

#include "2Dc-deque.h"
#include "2Dc-window_maged.c"

#if defined(RELAXATION_ANALYSIS)
	#include "semantic-relaxation_analysis-deque.c"
#endif

void thread_init(int thread_id)
{
	/*************relaxation code****************************/
	/*create thread local windows*/
	thread_Window = (window_t *) calloc(1, sizeof(window_t));
	new_window = (window_t *) calloc(1, sizeof(window_t));

	/*initialise thread window max limit*/
	thread_Window->max = depth;

	/*initialise thread starting index*/
	thread_index=thread_id*(width/num_threads);
	/*********************************************************/
}
deque_t* create_deque()
{
	anchor_t* anchor;
	deque_t* deque;
	 /******************relaxation_bound k = (2depth)(width - 1)***************/
	if(width<2) width = 2;
	if(depth<1) depth = 1;
	if(relaxation_bound<1)relaxation_bound=1;

	depth = relaxation_bound /(3*(width - 1));
	if(depth<1) depth = 1;

	width = (relaxation_bound/(3*depth)) + 1;
	if(width<1)width=1;

	if(depth>=2) shift_up = shift_down = depth/2;
	else shift_up = shift_down = 1;

	/*create global windows*/
	global_Window = (window_t *) calloc(1, sizeof(window_t));

	/*initialise window max limit*/
	global_Window->max = depth;
	global_Window->version = 1;

	/*create an array of sub_structures (relaxation)*/
	if ((deque=(deque_t*) calloc(width, sizeof(deque_t)))==NULL)
    {
		perror("malloc");
		exit(1);
	}

	/*initialise each sub_structure*/
	int i;
	for(i=0;i<width;i++)
	{
		anchor = (anchor_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(anchor_t));
		anchor->left = NULL;
    	anchor->right = NULL;
		anchor->state = STATE_STABLE;
		deque[i].anchor=anchor;
	}

	#if defined(RELAXATION_ANALYSIS)
		initialise_relaxation_analysis();
	#endif
	return deque;
}

node_t* create_node(skey_t k, sval_t value)
{
    volatile node_t* new_node;
	#if GC == 1
		new_node = (volatile node_t*) ssmem_alloc(alloc, sizeof(node_t));
		#else
		new_node = (volatile node_t*) ssalloc(sizeof(node_t));
	#endif
    if (new_node == NULL)
	{
        perror("malloc in create node");
        exit(1);
	}
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->key = k;
    new_node->val = value;
	// NOTE: Explanation https://stackoverflow.com/questions/14950614/working-of-asm-volatile-memory
    asm volatile("" ::: "memory");
    return new_node;
}

anchor_t* create_anchor()
{
    volatile anchor_t* new_anchor;
	#if GC == 1
		new_anchor = (volatile anchor_t*) ssmem_alloc(alloc2, sizeof(anchor_t));
		#else
		new_anchor = (volatile anchor_t*) ssalloc(sizeof(anchor_t));
	#endif
    if (new_anchor == NULL)
	{
        perror("malloc in create node");
        exit(1);
	}
    new_anchor->left = NULL;
    new_anchor->right = NULL;
    new_anchor->state = STATE_STABLE;
	/*window relaxation counters*/
    new_anchor->count=0;
    /***************************/
    asm volatile("" ::: "memory");
    return new_anchor;
}

int push_left(deque_t* set, skey_t key, sval_t value)
{
	anchor_t *anchor;
	anchor_t *nextAnchor=NULL;
	node_t *node;
	deque_t* deque;

	#if defined(RELAXATION_ANALYSIS)
		value = generate_count_val();//generate incremental values for quality analysis
	#endif
	node = create_node(key, value);
	if(node==NULL)
	{
		/* Error handling */
		return 0;
	}

	contention=0;
	while(1)
	{
		if(nextAnchor==NULL)nextAnchor=create_anchor();
		while(1)
		{
			/********relaxation window********/
			window_put(set);
			anchor=anchor_ptr;
			deque=&set[thread_index];
			/*********************************/
			if(anchor==deque->anchor)break;
		}
		if(anchor->left==NULL)
		{
			*nextAnchor=*anchor;
			nextAnchor->count+=1;
			nextAnchor->left=node;
			nextAnchor->right=node;
			#if defined(RELAXATION_ANALYSIS)
			if(CasPutL_RA(&deque->anchor, anchor, nextAnchor, value))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				break;
			}
			my_put_cas_fail_count+=1;
			contention+=1;
		}
		else if(anchor->state==STATE_STABLE)
		{
			node->right=anchor->left;

			*nextAnchor=*anchor;
			nextAnchor->count+=1;
			nextAnchor->left=node;
			nextAnchor->state=STATE_LPUSH;
			#if defined(RELAXATION_ANALYSIS)
			if(CasPutL_RA(&deque->anchor, anchor, nextAnchor, value))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				anchor=nextAnchor;
				stabilize_left(anchor,deque);
				break;
			}
			my_put_cas_fail_count+=1;
			contention+=1;
		}
		else stabilize(anchor,deque);
	}
	#if GC == 1
		ssmem_free(alloc2, (void*) anchor);
	#endif
	return 1;
}

int push_right(deque_t* set, skey_t key, sval_t value)
{
	anchor_t *anchor;
	anchor_t *nextAnchor=NULL;
	node_t *node;
	deque_t* deque;

	#if defined(RELAXATION_ANALYSIS)
		value = generate_count_val();//generate incremental values for quality analysis
	#endif
	node = create_node(key, value);
	if(node==NULL)
	{
		/* Error handling */
		return 0;
	}

	contention=0;
	while(1)
	{
		if(nextAnchor==NULL)nextAnchor=create_anchor();
		while(1)
		{
			/********relaxation window********/
			window_put(set);
			anchor=anchor_ptr;
			deque=&set[thread_index];
			/*********************************/
			if(anchor==deque->anchor)break;
		}
		if(anchor->right==NULL)
		{
			*nextAnchor=*anchor;
			nextAnchor->count+=1;
			nextAnchor->left=node;
			nextAnchor->right=node;
			#if defined(RELAXATION_ANALYSIS)
			if(CasPutR_RA(&deque->anchor, anchor, nextAnchor, value))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				break;
			}
			my_put_cas_fail_count+=1;
			contention+=1;
		}
		else if(anchor->state==STATE_STABLE)
		{
			node->left=anchor->right;

			*nextAnchor=*anchor;
			nextAnchor->count+=1;
			nextAnchor->right=node;
			nextAnchor->state=STATE_RPUSH;
			#if defined(RELAXATION_ANALYSIS)
			if(CasPutR_RA(&deque->anchor, anchor, nextAnchor, value))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				anchor=nextAnchor;
				stabilize_right(anchor,deque);
				break;
			}
			my_put_cas_fail_count+=1;
			contention+=1;
		}
		else stabilize(anchor,deque);
	}
	#if GC == 1
		ssmem_free(alloc2, (void*) anchor);
	#endif
	return 1;
}

sval_t pop_right(deque_t* set)
{
	sval_t value;
	anchor_t *anchor;
	anchor_t *nextAnchor=NULL;
	node_t *prev;
	node_t *node;
	deque_t* deque;

	contention=0;
	while(1)
	{
		if(nextAnchor==NULL)nextAnchor=create_anchor();
		while(1)
		{
			/********relaxation window********/
			window_get(set);
			if(empty_check==1)
			{
				#if defined(RELAXATION_ANALYSIS)
					relaxation_null_remove((1));
				#endif
				my_null_count+=1;
				return NULL;
			}
			anchor=anchor_ptr;
			deque=&set[thread_index];
			/*********************************/
			if(anchor==deque->anchor)break;
		}
		if(anchor->right==anchor->left)
		{
			*nextAnchor=*anchor;
			nextAnchor->count-=1;
			nextAnchor->left=NULL;
			nextAnchor->right=NULL;
			#if defined(RELAXATION_ANALYSIS)
			if(CasGetR_RA(&deque->anchor, anchor, nextAnchor, anchor->right))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				node=anchor->right;
				break;
			}
			contention+=1;
			my_put_cas_fail_count+=1;
		}
		else if(anchor->state==STATE_STABLE)
		{
			if(deque->anchor!=anchor)continue;
			prev=anchor->right->left;

			*nextAnchor=*anchor;
			nextAnchor->count-=1;
			nextAnchor->right=prev;
			#if defined(RELAXATION_ANALYSIS)
			if(CasGetR_RA(&deque->anchor, anchor, nextAnchor, anchor->right))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				node=anchor->right;
				break;
			}
			contention+=1;
			my_put_cas_fail_count+=1;
		}
		else stabilize(anchor,deque);
	}
	value=node->val;
	#if GC == 1
		ssmem_free(alloc, (void*) node);
		ssmem_free(alloc2, (void*) anchor);
	#endif
	return value;
}

sval_t pop_left(deque_t* set)
{
	sval_t value;
	anchor_t *anchor;
	anchor_t *nextAnchor=NULL;
	node_t *prev;
	node_t *node;
	deque_t* deque;

	contention=0;
	while(1)
	{
		if(nextAnchor==NULL)nextAnchor=create_anchor();
		while(1)
		{
			/********relaxation window********/
			window_get(set);
			if(empty_check==1)
			{
				#if defined(RELAXATION_ANALYSIS)
					relaxation_null_remove((1));
				#endif
				my_null_count+=1;
				return NULL;
			}
			anchor=anchor_ptr;
			deque=&set[thread_index];
			/*********************************/
			if(anchor==deque->anchor)break;
		}
		if(anchor->right==anchor->left)
		{
			*nextAnchor=*anchor;
			nextAnchor->count-=1;
			nextAnchor->left=NULL;
			nextAnchor->right=NULL;
			#if defined(RELAXATION_ANALYSIS)
			if(CasGetL_RA(&deque->anchor, anchor, nextAnchor, anchor->left))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				node=anchor->left;
				break;
			}
			my_get_cas_fail_count+=1;
			contention+=1;
		}
		else if(anchor->state==STATE_STABLE)
		{
			if(deque->anchor!=anchor)continue;
			prev=anchor->left->right;

			*nextAnchor=*anchor;
			nextAnchor->count-=1;
			nextAnchor->left=prev;
			#if defined(RELAXATION_ANALYSIS)
			if(CasGetL_RA(&deque->anchor, anchor, nextAnchor, anchor->left))
			#else
			if(CAS_BOOL(&deque->anchor,anchor,nextAnchor))
			#endif
			{
				node=anchor->left;
				break;
			}
			my_get_cas_fail_count+=1;
			contention+=1;
		}
		else stabilize(anchor,deque);
	}
	value=node->val;
	#if GC == 1
		ssmem_free(alloc, (void*) node);
		ssmem_free(alloc2, (void*) anchor);
	#endif
	return value;
}

void stabilize(anchor_t *anchor, deque_t *deque)
{
	if(anchor->state==STATE_RPUSH)stabilize_right(anchor,deque);
	else stabilize_left(anchor,deque);
}

void stabilize_left(anchor_t *anchor, deque_t *deque)
{
	anchor_t *nextAnchor;
	node_t *prevnext;
	node_t *prev;

	if(deque->anchor!=anchor) return;
	prev=anchor->left->right;
	if(deque->anchor!=anchor) return;
	prevnext=prev->left;
	if(prevnext!=anchor->left)
	{
		if(deque->anchor!=anchor) return;
		if(!CAS_BOOL(&prev->left,prevnext,anchor->left))
		{
			contention+=1;
			return;
		}
	}

	nextAnchor=create_anchor();
	*nextAnchor=*anchor;
	nextAnchor->state=STATE_STABLE;
	if(!CAS_BOOL(&deque->anchor,anchor,nextAnchor))
	{
		contention+=1;
		#if GC == 1
			ssmem_free(alloc2, (void*) nextAnchor);
		#endif
	}
}

void stabilize_right(anchor_t *anchor, deque_t *deque)
{
	anchor_t *nextAnchor;
	node_t *prevnext;
	node_t *prev;

	if(deque->anchor!=anchor) return;
	prev=anchor->right->left;
	if(deque->anchor!=anchor) return;

	prevnext=prev->right;
	if(prevnext!=anchor->right)
	{
		if(deque->anchor!=anchor) return;
		if(!CAS_BOOL(&prev->right,prevnext,anchor->right))
		{
			contention+=1;
			return;
		}
	}

	nextAnchor=create_anchor();
	*nextAnchor=*anchor;
	nextAnchor->state=STATE_STABLE;
	if(!CAS_BOOL(&deque->anchor,anchor,nextAnchor))
	{
		contention+=1;
		#if GC == 1
			ssmem_free(alloc2, (void*) nextAnchor);
		#endif
	}
}

int deque_size_2D(deque_t *deque)
{
	int i;
	int size=0;
	deque_t* sub_deque;
	for(i=0;i<width;i++)
	{
		sub_deque = &deque[i];
		size += deque_size(sub_deque);
	}
	return size;
}

int deque_size(deque_t *deque)
{
	int size=0;
	anchor_t *anchor;
	node_t * node;

	size_retry:
	while(1)
	{
		anchor=deque->anchor;
		if(anchor->state!=STATE_STABLE)
		{
			stabilize(anchor,deque);
		}
		if(anchor==deque->anchor)break;
	}
	node = anchor->left;
	while(node != NULL)
	{
		if(anchor!=deque->anchor)
		{
			size=0;
			goto size_retry;
		}
		size++;
		if(anchor->right==node)break;//stop at this node since the node right pointer is never updated to null by the pop_right
		node=node->right;
	}
	return size;
}

