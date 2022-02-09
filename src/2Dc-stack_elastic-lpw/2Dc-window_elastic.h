#ifndef TWODC_WINDOW_ELASTIC_H
#define TWODC_WINDOW_ELASTIC_H

#include "types.h"

typedef ALIGNED(CACHE_LINE_SIZE) struct window_descriptor
{
	row_t max;
	depth_t depth;
	width_t get_width;
	width_t put_width;
	width_t old_put_width;
	version_t version;
	uint16_t last_shift; // 0: up, 1: down (TODO: add way to remove things at the bottommost window. Currently, the last lateral will stay forever, slowing down some stacks. It can be fixed by setting this to eg 2 when shifting down with Win_min already at bottom.)

} window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct window_struct
{
	window_t content;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(window_t)];
} padded_window_t;

/*window variables*/
volatile padded_window_t global_Window;

__thread window_t thread_Window;
__thread uint64_t thread_put_index;
__thread uint64_t thread_get_index;

/*functions, descriptor_t defined within the data structure header file*/
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t get_window(DS_TYPE* set, uint8_t contention);
uint64_t random_index(width_t width);
void initialize_global_window(depth_t depth, width_t width);

#endif
