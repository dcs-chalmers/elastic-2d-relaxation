
#include "relaxation_2dd-window.h"

descriptor_t put_window(DS_TYPE* set, uint8_t contention)
{
	uint64_t hops, random;
	hops = random = 0;
	descriptor_t descriptor;
	
	if(contention == 1)
	{		
		thread_index == random_index(set);
		contention = 0;
	}
	
	if(thread_PWindow.max != global_PWindow.max) 
	{
		thread_PWindow.max = global_PWindow.max; 
	}
	while(1)
	{
		//shift window
		if(hops == set->width) 
		{
			if(thread_PWindow.max == global_PWindow.max)
			{
				new_window.max = thread_PWindow.max + set->depth;
				if(CAS(&global_PWindow.max,thread_PWindow.max,new_window.max))
				{
					my_slide_count+=1;
				}
			}
			thread_PWindow.max = global_PWindow.max;
			hops = 0;
		}
		//read descriptor
		descriptor =  set->put_array[thread_index].descriptor;
		if(descriptor.put_count < global_PWindow.max)
		{
			return descriptor;
		}
		//hop 
		else if(thread_PWindow.max == global_PWindow.max)
		{
			if(random < set->random_hops)
			{
				thread_index = random_index(set); 
				random += 1;
			}
			else
			{
				thread_index += 1;
				if(thread_index == set->width)
				{
					thread_index=0;
				}
			}
			hops += 1;
			my_hop_count+=1;
		}
		//switch to current window
		else
		{
			thread_PWindow.max = global_PWindow.max; 
			hops = 0;
		}
	}
}

descriptor_t get_window(DS_TYPE* set, uint8_t contention)
{
	uint64_t hops, random;
	hops = random = 0; 
	descriptor_t descriptor;
	uint8_t notempty = 0;
	
	if(contention == 1)
	{		
		thread_index = random_index(set);
		contention = 0;
	}
	
	if(thread_GWindow.max != global_GWindow.max)
	{
		thread_GWindow.max = global_GWindow.max; 
		//notempty=0; 
	}
	while(1)
	{
		//shift window
		if(hops == set->width)
		{
			if(thread_GWindow.max == global_GWindow.max)
			{
				new_window.max = thread_GWindow.max + set->depth;
				if(CAS(&global_GWindow.max,thread_GWindow.max,new_window.max))
				{
					my_slide_count+=1;
				}
			}
			thread_GWindow.max = global_GWindow.max; 
			hops = notempty = 0;
		}
		//read descriptor
		descriptor =  set->get_array[thread_index].descriptor;
		//if(descriptor.get_count < global_GWindow.max && ((set->put_array[thread_index].descriptor.put_count - descriptor.get_count)>0)) //with emptiness check, do not return empty substructure
		if(descriptor.get_count < global_GWindow.max) //can return empty substructure, no emptiness check
		{
			return descriptor;
		}
		//change index (hop) 
		else if(thread_GWindow.max == global_GWindow.max)
		{
			/* emptiness check *
			if((set->put_array[thread_index].descriptor.put_count - descriptor.get_count) > 0)
			{			
				notempty = 1;
			}
			/**/
			if(random < set->random_hops)
			{
				thread_index = random_index(set); 
				random += 1;
			}
			else
			{					
				hops += 1;
				/* emptiness check *
				if(hops == set->width && notempty == 0)
				{				
					//if all sub structures are empty within the same window return even if substructure is empty
					return descriptor;
				}
				*/
				thread_index += 1;
				if(thread_index == set->width)
				{
					thread_index=0;
				}
			}
			my_hop_count+=1;
		}
		//switch to current window
		else
		{			
			thread_GWindow.max = global_GWindow.max; 
			hops = notempty = 0; 
		}
	}
}

uint64_t random_index(DS_TYPE* set)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (set->width));
}