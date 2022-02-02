#include "lock_if.h"

/********* quality analysis declarations *********/
typedef struct relaxation_list_node
{
	sval_t value; 
	void* next;
} rl_node_t;
typedef struct double_word_size
{
	uint64_t x; 
	uint64_t y;
} double_word_t;
extern rl_node_t* rl_head;
extern rl_node_t* rl_tail;
extern sval_t *count_val;
extern size_t initial;
extern int relaxation_bound;
extern unsigned long semantic_distance_array_size;
extern unsigned long *semantic_distance_array;
extern ptlock_t relaxation_list_lock;

int cas_relaxation_add(uint64_t* address, uint64_t old_val, uint64_t new_val, void* value);
int cas_relaxation_delete(uint64_t*  address, uint64_t old_val, uint64_t new_val, void* value);
int cae_relaxation_add(double_word_t* address, double_word_t* old_val, double_word_t* new_val, void* value);
int cae_relaxation_delete(double_word_t*  address, double_word_t* old_val, double_word_t* new_val, void* value);
sval_t generate_count_val();
void relaxation_add(void* value);
void relaxation_remove(void* value);
int relaxation_null_remove(void* condition);
void unlock_relaxation_list();
void lock_relaxation_list();
void initialise_relaxation_analysis();
void print_relaxation_measurements();
void relaxation_list_size();