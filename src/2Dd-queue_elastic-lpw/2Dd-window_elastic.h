#ifndef TWODd_window_elastic
#define TWODd_window_elastic

typedef struct put_window_struct
{
	// Want to be able to use CAS, so max 128 bits in x86. It might be possible to fit in 64 bits, but it would depend on application, and require more care.
	union {
		struct {
			row_t max;
			depth_t depth; 		// Needed to not enqueue below the window if above a gap
			width_t width;		// The width in use
			width_t next_width;	// The width of the next window, included in window to be able to prepare the lateral before the shift.
		};
		struct {
			// This representation is used to aviod having to use atomic reads of the window on x86 as we can utilize that word1 increases strictly monotonically
			uint64_t word1;
			uint64_t word2;
		}
	}
} put_window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct padded_put_window_struct
{
	put_window_t content;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(put_window_t)];
} padded_put_window_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct get_window_struct
{
	union {
		struct {
			row_t max;
			depth_t depth;
			width_t width;
		};
		struct {
			// This representation is used to aviod having to use atomic reads of the window on x86 as we can utilize that word1 increases strictly monotonically
			uint64_t word1;
			uint64_t word2;
		}
	}
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
__thread width_t thread_put_index;
__thread width_t thread_get_index;

/* functions */
descriptor_t put_window(DS_TYPE* set, uint8_t contention);
descriptor_t get_window(DS_TYPE* set, uint8_t contention);
width_t random_index(width_t width);
void initialize_global_window(depth_t depth, width_t width);

#endif
