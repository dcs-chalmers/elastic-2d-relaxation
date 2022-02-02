/**************************************************************************************
	quality check functions
****************************************************************************************/
void initialise_relaxation_analysis()
{
	semantic_distance_array_size=2048*10;
	semantic_distance_array = (unsigned long *) calloc(semantic_distance_array_size, sizeof(unsigned long));
}
sval_t generate_count_val()
{
	return IAF_U64(&count_val);
}
void lock_relaxation_list()
{
	#ifndef RELAXATION_SEMANTICS
		printf("\n ****************Error: Semantics not defined (RELAXATION_STRUCT 1 for LIFO 2 for FIFO)************\n\n");
		exit(1);
	#elif RELAXATION_SEMANTICS != 1 && RELAXATION_SEMANTICS != 2 && RELAXATION_SEMANTICS != 3
		printf("\n ****************Error: Defined semantics unknown (RELAXATION_STRUCT 1 for LIFO 2 for FIFO 3 for Counter)************\n\n");
		exit(1);
	#endif
	LOCK_A(&relaxation_list_lock);
}
void unlock_relaxation_list()
{
	UNLOCK_A(&relaxation_list_lock);
}
int cas_relaxation_add(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAS(address_v, old_v, new_v))
	{
		relaxation_add(value);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int cas_relaxation_remove(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	#if RELAXATION_SEMANTICS == 3
		if (CAS(address_v, old_v, new_v))
		{
			relaxation_remove(node_ptr);
			success=1;
		}
	#else
		#if RELAXATION_SEMANTICS == 1
			mstack_node_t* node=node_ptr;
		#elif RELAXATION_SEMANTICS == 2
			queue_node_t* node=node_ptr;
		#endif
		if (CAS(address_v, old_v, new_v))
		{
			relaxation_remove(node->val);
			success=1;
		}
	#endif
	unlock_relaxation_list();
	return success;
}
int cae_relaxation_add(double_word_t* address_v, double_word_t* old_v, double_word_t* new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAE(address_v, old_v, new_v))
	{
		relaxation_add(value);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int cae_relaxation_remove(double_word_t* address_v, double_word_t* old_v, double_word_t* new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	#if RELAXATION_SEMANTICS == 3
		if (CAE(address_v, old_v, new_v))
		{
			relaxation_remove(node_ptr);
			success=1;
		}
	#else
		#if RELAXATION_SEMANTICS == 1
			mstack_node_t* node=node_ptr;
		#elif RELAXATION_SEMANTICS == 2
			queue_node_t* node=node_ptr;
		#endif
		if (CAE(address_v, old_v, new_v))
		{
			relaxation_remove(node->val);
			success=1;
		}
	#endif
	unlock_relaxation_list();
	return success;
}
void relaxation_add(void* value)
{
	#if RELAXATION_SEMANTICS == 3
		volatile long semantic_distance=0;
		IAF_U64(&count_val);
		if(count_val>=value)semantic_distance=(int64_t)count_val-(int64_t)value;
		else if(count_val<value)semantic_distance=(int64_t)value-(int64_t)count_val;
		FAI_U64(&semantic_distance_array[semantic_distance]);
	#else
		rl_node_t* count_node = (rl_node_t*) calloc(1,sizeof(rl_node_t));
		count_node->value=value;
		if(rl_head==NULL)
		{
			rl_tail = count_node;
			rl_head = count_node;
		}
		else
		{
			#if RELAXATION_SEMANTICS == 1
				count_node->next = rl_head;
				rl_head = count_node;
			#elif RELAXATION_SEMANTICS == 2
				rl_tail->next = count_node;
				rl_tail = count_node;
			#endif	
		}
	#endif
}
int relaxation_null_remove(void* condition)
{
	int success=0;
	lock_relaxation_list();
	if(condition)
	{
		relaxation_remove(0);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
void relaxation_remove(void* value)
{
	volatile long semantic_distance=0;
	int volatile found=0;
	#if RELAXATION_SEMANTICS == 3
		if(count_val>0)DAF_U64(&count_val);
		if(count_val>=value)semantic_distance=(int64_t)count_val-(int64_t)value;
		else if(count_val<value)semantic_distance=(int64_t)value-(int64_t)count_val;
		FAI_U64(&semantic_distance_array[semantic_distance]);
	#else
		if(rl_head!=NULL)
		{
			rl_node_t* count_node=rl_head;
			rl_node_t* previous_node=NULL;
			while(count_node!=NULL)
			{
				if(count_node->value==value)
				{
					if(semantic_distance==0)rl_head=count_node->next;//unlink node from list
					else if(count_node==rl_tail)
					{
						previous_node->next=count_node->next;//unlink node from list
						rl_tail = previous_node;
					}
					else previous_node->next=count_node->next;//unlink node from list
					free(count_node);
					found=1;
					break;
				}
				else
				{
					semantic_distance+=1;
					previous_node = count_node;
					count_node = count_node->next;
				}
			}
			if(found==0&&value!=0)
			{
				printf("Error: Value %lu not on relaxation list\n",value);
				//*(int *)1=0;
				exit(1);
			}
			FAI_U64(&semantic_distance_array[semantic_distance]);
		}
		else if(value==0)
		{
			FAI_U64(&semantic_distance_array[semantic_distance]);
		}
		else
			printf("Error: List is empty! Value %lu not on relaxation list\n",value);
		//test relaxation_bound
		//printf("popstack_pushcount_popwin_popcount, %zu,%zu,%zu,%zu\n", push_slot, stack_array[push_slot].descriptor.push_count, pop_window->global_counter.count,stack_array[push_slot].descriptor.pop_count);
		/*if(semantic_distance>relaxation_bound)
		{
			int i;
			for(i=0; i < (stack_array_size);i++)
			{
				printf("Push_Pop_window, %zu_%zu_%zu\n", stack_array[i].descriptor.push_count, stack_array[i].descriptor.pop_count, pop_window->global_counter.count);
			}
			printf("stack =  , %zu\n", push_slot);
			printf("value =  , %zu\n", value);
			exit(1);
		}*/
	#endif
}
void relaxation_list_size()
{
	unsigned long rl_size=0;
	rl_node_t* count_node=rl_head;
	while(count_node!=NULL)
	{
		rl_size+=1;
		count_node = count_node->next;
	}
	printf("Relaxation list size......... %zu \n", rl_size);
}
void print_relaxation_measurements()
{
	printf("Quality , Frequency \n");
	unsigned long total_f;
	long fi;
	for(fi=0,total_f=0;fi<semantic_distance_array_size;fi++)
	{
		total_f += semantic_distance_array[fi];
		printf("%zu , %zu \n", fi, semantic_distance_array[fi]);		
	}
	printf("Total , %zu \n", total_f);
	printf("Relaxation Bound , %d \n", relaxation_bound);
	printf("RELAXATION SEMANTICS , %d \n", RELAXATION_SEMANTICS);
	//relaxation_list_size();
}
/******************************************************************************/