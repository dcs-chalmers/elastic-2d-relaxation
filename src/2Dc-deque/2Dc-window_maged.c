
#include "2Dc-window_maged.h"

void window_put(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	anchor_ptr = NULL;

	if(contention==1)
	{
		thread_index==random_index();
		DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_Window->version != global_Window->version)
	{
		memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
	}
	while(1)
	{
		//shift window
		if(hops == width)
		{
			if(thread_Window->version == global_Window->version)
			{
				memcpy(&new_window->max,&thread_Window->max,sizeof(uint128_t));
				new_window->max += shift_up;
				new_window->version += 1;
				if(CAE((uint128_t*)&global_Window->max,(uint128_t*)&thread_Window->max, (uint128_t*)&new_window->max))my_window_count+=1;
			}
			memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
			hops = 0;
		}
		anchor_ptr = set[thread_index].anchor;	// Set pointer to sub-stack
		if(anchor_ptr->count < global_Window->max)
		{
			return;
		}
		//change index (hop)
		if(thread_Window->version == global_Window->version)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
				my_hop_count+=1;
			}
			else
			{
				if(thread_index == width - 1)thread_index=0;
				else thread_index += 1;
				hops += 1;
			}
			my_hop_count+=1;
		}
		//switch to current window
		else
		{
			memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
			hops = 0;
		}
	}
}

void window_get(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	empty_check=0;
	anchor_ptr = NULL;
	notempty=0;

	if(contention==1)
	{
		thread_index==random_index();
		DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_Window->version != global_Window->version)
	{
		memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
	}
	while(1)
	{
		//shift window
		if(hops == width)
		{
			if(notempty==0)
			{
				empty_check=1;
				return ;
			}
			else if(thread_Window->version == global_Window->version)
			{
				memcpy(&new_window->max,&thread_Window->max,sizeof(uint128_t));
				new_window->max -= shift_down;
				new_window->version += 1;
				if(CAE((uint128_t*)&global_Window->max,(uint128_t*)&thread_Window->max, (uint128_t*)&new_window->max))my_window_count+=1;
			}
			memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
			hops = 0;
			notempty=0;
		}
		anchor_ptr = set[thread_index].anchor;
		if(anchor_ptr->count > (global_Window->max - depth) && anchor_ptr->left!=NULL )
		{
			return;
		}
		//change index (hop)
		else if(thread_Window->version == global_Window->version)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
				my_hop_count+=1;
			}
			else
			{
				if(thread_index == width - 1) thread_index=0;
				else thread_index += 1;
				hops += 1;
			}
			if(anchor_ptr!=NULL&&anchor_ptr->left!=NULL) notempty=1;
			my_hop_count+=1;
		}
		//switch to current window
		else
		{
			memcpy(&thread_Window->max,&global_Window->max,sizeof(uint128_t));
			hops = 0;
			notempty=0;
		}
	}
}

int random_index()
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (width));
}