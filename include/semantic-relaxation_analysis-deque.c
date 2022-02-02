/**************************************************************************************
	quality check functions
	left is head
	right is tail
****************************************************************************************/
#include "lock_if.h"
typedef struct relaxation_list_node
{
	sval_t value; 
	void* next;
	void* prev;
} rl_node_t;

typedef unsigned __int128 uint128_t;

rl_node_t* rl_head;
rl_node_t* rl_tail;
sval_t *count_val;
unsigned long semantic_distance_array_size;
unsigned long *semantic_distance_array;
volatile ptlock_t relaxation_list_lock;

sval_t generate_count_val();
int CasPutR_RA(uint64_t* address, uint64_t old_val, uint64_t new_val, void* value);
int CasGetR_RA(uint64_t*  address, uint64_t old_val, uint64_t new_val, void* value);
int CasPutL_RA(uint64_t* address, uint64_t old_val, uint64_t new_val, void* value);
int CasGetL_RA(uint64_t*  address, uint64_t old_val, uint64_t new_val, void* value);

int CaePutR_RA(uint128_t* address, uint128_t* old_val, uint128_t* new_val, void* value);
int CaeGetR_RA(uint128_t*  address, uint128_t* old_val, uint128_t* new_val, void* value);
int CaePutL_RA(uint128_t* address, uint128_t* old_val, uint128_t* new_val, void* value);
int CaeGetL_RA(uint128_t*  address, uint128_t* old_val, uint128_t* new_val, void* value);

void PutL_RA(void* value);
void GetL_RA(void* value);
void PutR_RA(void* value);
void GetR_RA(void* value);

int relaxation_null_remove(void* condition);
void unlock_relaxation_list();
void lock_relaxation_list();
void initialise_relaxation_analysis();
void print_relaxation_measurements();
void relaxation_list_size();
/****************************************************************************************/

void initialise_relaxation_analysis()
{
	count_val=0;
	
	semantic_distance_array_size=2048*10;
	semantic_distance_array = (unsigned long *) calloc(semantic_distance_array_size, sizeof(unsigned long));
}
sval_t generate_count_val()
{
	return IAF_U64(&count_val);
}
void lock_relaxation_list()
{
	LOCK_A(&relaxation_list_lock);
}
void unlock_relaxation_list()
{
	UNLOCK_A(&relaxation_list_lock);
}
void relaxation_list_size()
{
	unsigned long rl_size=0;
	rl_node_t* rl_node=rl_head;
	while(rl_node!=NULL)
	{
		rl_size+=1;
		rl_node = rl_node->next;
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
	//relaxation_list_size();
}
/****************************************************************************************/

int CasPutR_RA(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAS_BOOL(address_v, old_v, new_v))
	{
		PutR_RA(value);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int CasPutL_RA(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAS_BOOL(address_v, old_v, new_v))
	{
		PutL_RA(value);
		success=1;
	}
	unlock_relaxation_list(); 
	return success;
}
int CasGetR_RA(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	node_t* node=node_ptr;
	if (CAS_BOOL(address_v, old_v, new_v))
	{
		GetR_RA(node->val);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int CasGetL_RA(uint64_t* address_v, uint64_t old_v, uint64_t new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	node_t* node=node_ptr;
	if (CAS_BOOL(address_v, old_v, new_v))
	{
		GetL_RA(node->val);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}

int CaePutR_RA(uint128_t* address_v, uint128_t* old_v, uint128_t* new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAE(address_v, old_v, new_v))
	{
		PutR_RA(value);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int CaePutL_RA(uint128_t* address_v, uint128_t* old_v, uint128_t* new_v, void* value)
{
	lock_relaxation_list();
	int success=0;
	if (CAE(address_v, old_v, new_v))
	{
		PutL_RA(value);
		success=1;
	}
	unlock_relaxation_list(); 
	return success;
}
int CaeGetR_RA(uint128_t* address_v, uint128_t* old_v, uint128_t* new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	node_t* node=node_ptr;
	if (CAE(address_v, old_v, new_v))
	{
		GetR_RA(node->val);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
int CaeGetL_RA(uint128_t* address_v, uint128_t* old_v, uint128_t* new_v, void* node_ptr)
{
	lock_relaxation_list();
	int success=0;
	node_t* node=node_ptr;
	if (CAE(address_v, old_v, new_v))
	{
		GetL_RA(node->val);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}

void PutR_RA(void* value)
{
	rl_node_t* rl_node = (rl_node_t*) calloc(1,sizeof(rl_node_t));
	rl_node->value=value;
	if(rl_tail==NULL)
	{
		rl_tail = rl_node;
		rl_head = rl_node;
	}
	else
	{
		rl_node->prev=rl_tail;
		rl_tail->next = rl_node;
		rl_tail = rl_node;
	}
}
void PutL_RA(void* value)
{
	rl_node_t* rl_node = (rl_node_t*) calloc(1,sizeof(rl_node_t));
	rl_node->value=value;
	if(rl_head==NULL)
	{
		rl_tail = rl_node;
		rl_head = rl_node;
	}
	else
	{
		rl_node->next = rl_head; 
		rl_head->prev = rl_node;
		rl_head = rl_node;	
	}
}
void GetR_RA(void* value)
{
	volatile long semantic_distance=0;
	int volatile found=0;
	if(rl_tail!=NULL)
	{
		rl_node_t* rl_node=rl_tail;
		rl_node_t* next_node=NULL;
		rl_node_t* previous_node=NULL;
		while(rl_node!=NULL)
		{
			if(rl_node->value==value)
			{
				if(semantic_distance==0)
				{
					rl_tail=rl_node->prev;//unlink node from list
					if(rl_node==rl_head) rl_head=NULL;
					else rl_tail->next=NULL; //rl_node->next;//unlink node from deleted node
				}
				else if(rl_node==rl_head)
				{
					rl_head = next_node;//unlink node from list
					next_node->prev = NULL;//rl_node->prev;//unlink node from deleted node
				}
				else 
				{
					//unlink node from list
					previous_node = rl_node->prev;
					next_node->prev = previous_node;
					previous_node->next = next_node;
				}
				free(rl_node);
				found=1;
				break;
			}
			else
			{
				semantic_distance+=1;
				next_node = rl_node;
				rl_node = rl_node->prev;
			}
		}
		if(found==0&&value!=0)
		{
			printf("Error: Value %lu not on relaxation list\n",value);
			//*(int *)1=0;
			//relaxation_list_size(); //testing
			exit(1);
		}
		//if(semantic_distance>relaxation_bound){printf("Distance R............. %d \n",semantic_distance);relaxation_list_size(),*(int *)1=0;}//testing
		FAI_U64(&semantic_distance_array[semantic_distance]);
	}
	else if(value==0)
	{
		FAI_U64(&semantic_distance_array[semantic_distance]);
	}
	else
		printf("Error: List is empty! Value %lu not on relaxation list\n",value);
}
void GetL_RA(void* value)
{
	volatile long semantic_distance=0;
	int volatile found=0;
	if(rl_head!=NULL)
	{
		rl_node_t* rl_node=rl_head;
		rl_node_t* previous_node=NULL;
		rl_node_t* next_node=NULL;
		while(rl_node!=NULL)
		{
			if(rl_node->value==value)
			{
				if(semantic_distance==0)
				{
					rl_head=rl_node->next;//unlink node from list
					if(rl_node==rl_tail) rl_tail=NULL;
					else rl_head->prev=NULL; //rl_node->prev;//unlink node from deleted node
				}
				else if(rl_node==rl_tail)
				{
					rl_tail = previous_node;//unlink node from list
					previous_node->next=NULL;//rl_node->next;//unlink node from deleted node
				}
				else
				{
					//unlink node from list
					next_node = rl_node->next;
					previous_node->next = next_node;
					next_node->prev = previous_node;
				}
				free(rl_node);
				found=1;
				break;
			}
			else
			{
				semantic_distance+=1;
				previous_node = rl_node;
				rl_node = rl_node->next;
			}
		}
		if(found==0&&value!=0)
		{
			printf("Error: Value %lu not on relaxation list\n",value);
			//*(int *)1=0;
			exit(1);
		}
		//if(semantic_distance>relaxation_bound){printf("Distance L............. %d \n",semantic_distance);relaxation_list_size(),*(int *)1=0;}//testing
		FAI_U64(&semantic_distance_array[semantic_distance]);
	}
	else if(value==0)
	{
		FAI_U64(&semantic_distance_array[semantic_distance]);
	}
	else
		printf("Error: List is empty! Value %lu not on relaxation list\n",value);
}
int relaxation_null_remove(void* condition)
{
	int success=0;
	lock_relaxation_list();
	if(condition)
	{
		GetL_RA(0);
		success=1;
	}
	unlock_relaxation_list();
	return success;
}
/******************************************************************************/