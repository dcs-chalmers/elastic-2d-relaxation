#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "lock_if.h"
#include "common.h"
#include "atomic_ops_if.h"
#include "ssalloc.h"
#include "ssmem.h"

#define STATE_STABLE 0
#define STATE_RPUSH 1
#define STATE_LPUSH 2

#define DS_ADD_R(s,k,v)       	push_right(s,k,v)
#define DS_ADD_L(s,k,v)       	push_left(s,k,v)
#define DS_REMOVE_R(s)    		pop_right(s)
#define DS_REMOVE_L(s)    		pop_left(s)
#define DS_SIZE(s)          	deque_size_2D(s)
#define DS_THREAD_LOCAL(t)      thread_init(t)
#define DS_NEW()                create_deque()

#define DS_TYPE             deque_t
#define DS_NODE             node_t
#define DS_KEY              skey_t

extern __thread ssmem_allocator_t* alloc;
extern __thread ssmem_allocator_t* alloc2;

typedef ALIGNED(CACHE_LINE_SIZE) struct node_type
{
    sval_t val;
    skey_t key;
    struct node_t* volatile right;
    struct node_t* volatile left;
    uint8_t padding[CACHE_LINE_SIZE - (8*4)];
} node_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct anchor_struct
{
    uint64_t state;
    node_t* volatile right;
    node_t* volatile left;
    /*window relaxation counters*/
    uint64_t count;
    /***************************/
    uint8_t padding[CACHE_LINE_SIZE - (8*4)];
} anchor_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct deque_struct
{
    anchor_t* volatile anchor;
    uint8_t padding1[CACHE_LINE_SIZE - sizeof(anchor_t*)];
} deque_t;

deque_t* create_deque();
node_t* create_node(skey_t k, sval_t value);
anchor_t* create_anchor();
void thread_init(int thread_id);

int push_left(deque_t* set, skey_t k, sval_t value);
int push_right(deque_t* set, skey_t k, sval_t value);
sval_t pop_left(deque_t* set);
sval_t pop_right(deque_t* set);
int deque_size(deque_t* set);

static void stabilize(anchor_t *anchor, deque_t* deque);
static void stabilize_left(anchor_t *anchor, deque_t* deque) ;
static void stabilize_right(anchor_t *anchor, deque_t* deque);

extern size_t num_threads;
extern __thread unsigned long my_put_cas_fail_count;
extern __thread unsigned long my_get_cas_fail_count;
extern __thread unsigned long my_null_count;
extern __thread unsigned long my_hop_count;
extern __thread unsigned long my_window_count;

/************relaxation code************************/
int deque_size_2D(deque_t* set);
/**************************************************/