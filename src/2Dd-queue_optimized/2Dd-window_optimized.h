#ifndef TWODd_window_elastic
#define TWODd_window_elastic

typedef struct window_struct
{
	row_t max;
} window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct padded_window_struct
{
	window_t content;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(window_t)];
} padded_window_t;


/*window variables*/
volatile padded_window_t global_PWindow;
volatile padded_window_t global_GWindow;

__thread window_t thread_PWindow;
__thread window_t thread_GWindow;
__thread depth_t thread_depth;
__thread width_t thread_width;
__thread width_t thread_put_index;
__thread width_t thread_get_index;

/* functions */
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t get_window(DS_TYPE* set, uint8_t contention);
width_t random_index(width_t width);
void initialize_global_window(depth_t depth, width_t width);
void ds_thread_init(DS_TYPE* set);

#endif
