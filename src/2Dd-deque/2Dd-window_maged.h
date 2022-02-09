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
	uint8_t padding[CACHE_LINE_SIZE - sizeof(uint64_t)];
}window_t;

extern int width;
extern int depth;
extern int relaxation_bound;

__thread anchor_t* anchor_ptr;
__thread int contention;

__thread int thread_index;

/******emptiness check****/
__thread int empty_check;
__thread int notempty;

/*index mapping variables*/
__thread int* PLMap_array;
__thread int* GLMap_array;
__thread int* PRMap_array;
__thread int* GRMap_array;
__thread int PL_full;
__thread int PR_full;
__thread int GL_full;
__thread int GR_full;

/*window variables*/
window_t* global_PLWindow;
window_t* global_PRWindow;
window_t* global_GLWindow;
window_t* global_GRWindow;

__thread window_t* thread_PLWindow;
__thread window_t* thread_PRWindow;
__thread window_t* thread_GLWindow;
__thread window_t* thread_GRWindow;
__thread window_t* new_window;

/*functions*/
void put_left_window(DS_TYPE* set);
void put_right_window(DS_TYPE* set);
void get_left_window(DS_TYPE* set);
void get_right_window(DS_TYPE* set);
int random_index();