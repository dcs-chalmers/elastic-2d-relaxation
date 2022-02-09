#ifndef TWODd_window
#define TWODd_window

typedef struct put_window_struct
{
	uint64_t max;
} put_window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct padded_put_window_struct
{
	put_window_t content;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(put_window_t)];
} padded_put_window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct get_window_struct
{
	uint64_t max;
} get_window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct padded_get_window_struct
{
	get_window_t content;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(get_window_t)];
} padded_get_window_t;

/*window variables*/
volatile padded_put_window_t global_PWindow;
volatile padded_get_window_t global_GWindow;

__thread put_window_t thread_PWindow;
__thread get_window_t thread_GWindow;
__thread uint64_t thread_index;

/* functions */
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t get_window(DS_TYPE* set, uint8_t contention);
uint64_t random_index(uint32_t width);
void initialize_global_window(uint64_t depth);

#endif
