typedef ALIGNED(CACHE_LINE_SIZE) struct window_struct
{
	uint64_t max;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(uint64_t)]; 
}window_t;

/*window variables*/
window_t global_PWindow;
window_t global_GWindow;

__thread window_t thread_PWindow;
__thread window_t thread_GWindow;
__thread window_t new_window;
__thread uint64_t thread_index;

/*functions*/
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
uint64_t random_index(DS_TYPE* set);