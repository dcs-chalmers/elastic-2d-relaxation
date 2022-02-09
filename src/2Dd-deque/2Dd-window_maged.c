
#include "2Dd-window_maged.h"

void put_left_window(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	anchor_ptr = NULL;

	if(contention)
	{
		thread_index==random_index();
		//DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_PLWindow->max != global_PLWindow->max)
	{
		thread_PLWindow->max = global_PLWindow->max;
		PL_full=0;
	}
	while(1)
	{
		//shift window
		if(hops == width || PL_full == width)
		{
			if(thread_PLWindow->max == global_PLWindow->max)
			{
				new_window->max = thread_PLWindow->max + depth;
				if(CAS_BOOL(&global_PLWindow->max,thread_PLWindow->max,new_window->max))my_window_count+=1;
			}
			thread_PLWindow->max = global_PLWindow->max;
			PL_full=0;
			hops = 0;
		}
		if(PLMap_array[thread_index] < thread_PLWindow->max)
		{
			anchor_ptr = set[thread_index].anchor;
			if(anchor_ptr->PL_count < global_PLWindow->max)
			{
				return;
			}
			PL_full+=1;
			PLMap_array[thread_index]=anchor_ptr->PL_count;
		}
		//change index (hop)
		if(thread_PLWindow->max == global_PLWindow->max)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
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
			thread_PLWindow->max = global_PLWindow->max;
			hops = 0;
			PL_full=0;
		}
	}
}

void put_right_window(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	anchor_ptr = NULL;

	if(contention)
	{
		thread_index==random_index();
		//DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_PRWindow->max != global_PRWindow->max)
	{
		thread_PRWindow->max = global_PRWindow->max;
		PR_full=0;
	}
	while(1)
	{
		//shift window
		if(hops == width || PR_full == width)
		{
			if(thread_PRWindow->max == global_PRWindow->max)
			{
				new_window->max = thread_PRWindow->max + depth;
				if(CAS_BOOL(&global_PRWindow->max,thread_PRWindow->max,new_window->max))my_window_count+=1;
			}
			thread_PRWindow->max = global_PRWindow->max;
			PR_full=0;
			hops = 0;
		}
		if(PRMap_array[thread_index] < thread_PRWindow->max)
		{
			anchor_ptr = set[thread_index].anchor;
			if(anchor_ptr->PR_count < global_PRWindow->max)
			{
				return;
			}
			PR_full+=1;
			PRMap_array[thread_index]=anchor_ptr->PR_count;
		}
		//change index (hop)
		if(thread_PRWindow->max == global_PRWindow->max)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
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
			thread_PRWindow->max = global_PRWindow->max;
			hops = 0;
			PR_full=0;
		}
	}
}

void get_left_window(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	empty_check=0;
	anchor_ptr = NULL;

	if(contention)
	{
		thread_index==random_index();
		//DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_GLWindow->max != global_GLWindow->max)
	{
		thread_GLWindow->max = global_GLWindow->max;
		notempty=0;
		GL_full=0;
	}
	while(1)
	{
		//shift window
		if(hops == width || GL_full == width)
		{
			if(notempty==0)
			{
				empty_check=1;
				return ;
			}
			else if(thread_GLWindow->max == global_GLWindow->max)
			{
				new_window->max = thread_GLWindow->max + depth;
				if(CAS_BOOL(&global_GLWindow->max,thread_GLWindow->max,new_window->max))my_window_count+=1;
			}
			thread_GLWindow->max = global_GLWindow->max;
			hops = 0;
			notempty=0;
			GL_full=0;
		}
		if(GLMap_array[thread_index] < thread_GLWindow->max)
		{
			anchor_ptr = set[thread_index].anchor;
			if(anchor_ptr->GL_count < global_GLWindow->max)
			{
				if(anchor_ptr->left!=NULL)
				{
					return;
				}
			}
			else
			{
				GL_full+=1;
				GLMap_array[thread_index]=anchor_ptr->GL_count;
			}
		}
		//change index (hop)
		if(thread_GLWindow->max == global_GLWindow->max)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
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
			thread_GLWindow->max = global_GLWindow->max;
			hops = 0;
			notempty=0;
			GL_full=0;
		}
	}
}

void get_right_window(DS_TYPE* set)
{
	int hops = 0;
	int random = 0;
	empty_check=0;
	anchor_ptr = NULL;

	if(contention)
	{
		thread_index==random_index();
		//DO_PAUSE_EXP(contention);
		my_hop_count+=1;
	}

	if(thread_GRWindow->max != global_GRWindow->max)
	{
		thread_GRWindow->max = global_GRWindow->max;
		notempty=0;
		GR_full=0;
	}
	while(1)
	{
		//shift window
		if(hops == width || GR_full == width)
		{
			if(notempty==0)
			{
				empty_check=1;
				return ;
			}
			else if(thread_GRWindow->max == global_GRWindow->max)
			{
				new_window->max = thread_GRWindow->max + depth;
				if(CAS_BOOL(&global_GRWindow->max,thread_GRWindow->max,new_window->max))my_window_count+=1;
			}
			thread_GRWindow->max = global_GRWindow->max;
			hops = 0;
			notempty=0;
			GR_full=0;
		}
		if(GRMap_array[thread_index] < thread_GRWindow->max)
		{
			anchor_ptr = set[thread_index].anchor;
			if(anchor_ptr->GR_count < global_GRWindow->max)
			{
				if(anchor_ptr->right!=NULL)
				{
					return;
				}
			}
			else
			{
				GR_full+=1;
				GRMap_array[thread_index]=anchor_ptr->GR_count;
			}
		}
		//change index (hop)
		if(thread_GRWindow->max == global_GRWindow->max)
		{
			if(random < 2)
			{
				thread_index=random_index();
				random+=1;
			}
			else
			{
				if(thread_index == width - 1) thread_index=0;
				else thread_index += 1;
				hops += 1;
			}
			if(anchor_ptr!=NULL&&anchor_ptr->right!=NULL) notempty=1;
			my_hop_count+=1;
		}
		//switch to current window
		else
		{
			thread_GRWindow->max = global_GRWindow->max;
			hops = 0;
			notempty=0;
			GR_full=0;
		}
	}
}
int random_index()
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (width));
}