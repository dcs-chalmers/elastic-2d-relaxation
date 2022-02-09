/*
 * Author: Adones <adones@chalmers.se>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Discritors are synchronised globally (global descriptors )before each operatio is curried out
 */

#include "multi-counter_random-relaxed.h"

RETRY_STATS_VARS;

#include "latency.h"

#if LATENCY_PARSING == 1
	__thread size_t lat_parsing_get = 0;
	__thread size_t lat_parsing_put = 0;
	__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

__thread uint64_t thread_index;

counter_t* create_counter(uint64_t width)
{
	counter_t *set;
	if ((set = (counter_t*) ssalloc_aligned(CACHE_LINE_SIZE, sizeof(counter_t))) == NULL)
    {
		perror("malloc");
		exit(1);
    }
	set->array = (volatile index_t*)calloc(width,sizeof(index_t));//ssalloc(width);
	set->width = width;
	int i;
	for(i=0;i<width;i++)
	{
		set->array[i].count=0;
	}
	return set;
}

uint64_t increment(counter_t *set)
{
	int64_t count;
	descriptor_t descriptor, new_descriptor;
	while(1)
	{
		#if defined(NUM_CHOICES)
			descriptor = get_increment_index(set);
			new_descriptor.count = descriptor.count + 1;
			new_descriptor.version = descriptor.version + 1;
			if(CAE((descriptor_t*)&set->array[thread_index].count,&descriptor,&new_descriptor))
			{
				return new_descriptor.count * set->width;
			}
		#else
			thread_index = random_index(set);
			count = set->array[thread_index].count;
			if(CAS(&set->array[thread_index].count,count,(count+1)))
			{
				return ((count+1) * set->width);
			}
		#endif

		my_put_cas_fail_count+=1;
	}
}
uint64_t decrement(counter_t *set)
{
	int64_t s, count;
	descriptor_t descriptor, new_descriptor;

	while (1)
    {
		#if defined(NUM_CHOICES)
			descriptor = get_decrement_index(set);
			if(descriptor.count == 0)
			{
				my_null_count+=1;
				return 0;
			}
			else
			{
				new_descriptor.count = descriptor.count - 1;
				new_descriptor.version = descriptor.version + 1;
				if(CAE((descriptor_t*)&set->array[thread_index].count,&descriptor,&new_descriptor))
				{
					#if VALIDATESIZE==1
						return 1;
					#else
						return new_descriptor.count * set->width;
					#endif
				}
			}
		#else
			thread_index = random_index(set);
			count = set->array[thread_index].count;
			if(count != 0)
			{
				RETRY:
				if(CAS(&set->array[thread_index].count,count,(count-1)))
				{
					#if VALIDATESIZE==1
						return 1;
					#else
						return ((count-1) * set->width);
					#endif
				}
			}
			else
			{
				/* emptiness check
				for(s=0; s < set->width; s++)
				{
					count = set->array[s].count;
					if(count != 0)
					{
						thread_index = s;
						goto RETRY;
					}
				}
				*/
				my_null_count+=1;
				return 0;
			}
		#endif
		my_get_cas_fail_count+=1;
    }
}

#if defined(NUM_CHOICES)
	descriptor_t get_increment_index(counter_t *set)
	{
		descriptor_t descriptor, descriptor2;
		int64_t i, index2;
		thread_index = random_index(set);
		descriptor.version = set->array[thread_index].version;
		descriptor.count = set->array[thread_index].count;
		descriptor.count = set->array[thread_index].count;
		for(i=1;i < NUM_CHOICES;i++)
		{
			index2 = random_index(set);
			descriptor2.version = set->array[index2].version;
			descriptor2.count = set->array[index2].count;
			if(descriptor2.count < descriptor.count)
			{
				thread_index = index2;
				descriptor = descriptor2;
			}
		}
		return descriptor;
	}
	descriptor_t get_decrement_index(counter_t *set)
	{
		descriptor_t descriptor, descriptor2;
		int64_t i, s, index2;
		thread_index = random_index(set);
		RETRY:
		descriptor.version = set->array[thread_index].version;
		descriptor.count = set->array[thread_index].count;
		for(i=1; i < NUM_CHOICES; i++)
		{
			index2 = random_index(set);
			descriptor2.version = set->array[index2].version;
			descriptor2.count = set->array[index2].count;
			if(descriptor2.count > descriptor.count)
			{
				thread_index = index2;
				descriptor = descriptor2;
			}
		}
		/* emptiness check
		if(descriptor.count == 0)
		{
			for(s=0; s < set->width; s++)
			{
				if(set->array[s].count != 0)
				{
					thread_index = s;
					goto RETRY;
				}
			}
		}
		*/
		return descriptor;
	}
#endif

unsigned long random_index(counter_t *set)
{
	return (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (set->width));
}
size_t counter_size(counter_t *set)
{
	size_t size = 0;
	uint64_t i;
	for(i=0; i < set->width; i++)
	{
		size += set->array[i].count;
	}
	return size;
}