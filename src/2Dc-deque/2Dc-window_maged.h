#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "lock_if.h"
#include "common.h"
#include "atomic_ops_if.h"
#include "ssalloc.h"
#include "ssmem.h"

typedef ALIGNED(CACHE_LINE_SIZE) struct window_struct
{
	volatile uint64_t max;
	volatile uint64_t version;
	uint8_t padding[CACHE_LINE_SIZE - (sizeof(uint64_t)*2)];
}window_t;

typedef unsigned __int128 uint128_t;

extern int width;
extern int depth;
extern int shift_up;
extern int shift_down;
extern int relaxation_bound;
__thread anchor_t* anchor_ptr;
__thread int contention;

__thread int thread_index;

/******emptiness check****/
__thread int empty_check;
__thread int notempty;

/*window variables*/
window_t* global_Window;

__thread window_t* thread_Window;
__thread window_t* new_window;

/*functions*/
void window_put(DS_TYPE* set);
void window_get(DS_TYPE* set);
int random_index();