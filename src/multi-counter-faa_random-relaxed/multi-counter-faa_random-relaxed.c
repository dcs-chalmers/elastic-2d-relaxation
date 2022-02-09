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

#include "multi-counter-faa_random-relaxed.h"

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

	thread_index = random_index(set);

	count = IAF_U64(&set->array[thread_index].count);
	#if VALIDATESIZE==1
		return 1;
	#else
		return 1; //count;
	#endif
}
uint64_t decrement(counter_t *set)
{
	int64_t count;

	thread_index = random_index(set);
	count = DAF_U64(&set->array[thread_index].count);
	#if VALIDATESIZE==1
		return 1;
	#else
		return 1; //count;
	#endif
}

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