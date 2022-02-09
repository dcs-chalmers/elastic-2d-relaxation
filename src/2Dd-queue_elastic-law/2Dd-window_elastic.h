#ifndef TWODd_window_elastic
#define TWODd_window_elastic

#include "lateral_queue.h"


__thread lateral_node_t* thread_put_window;
__thread lateral_node_t* thread_get_window;
__thread lateral_node_t* thread_lhead_pointer;
__thread lateral_node_t* thread_ltail_pointer;
__thread width_t thread_put_index;
__thread width_t thread_get_index;

/* functions */
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t get_window(DS_TYPE* set, uint8_t contention);
width_t random_index(width_t width);

void init_thread_windows(DS_TYPE* set);

#endif
