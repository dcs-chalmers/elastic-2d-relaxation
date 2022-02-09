/*
 *   File: sl_ms.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description:
 *   sl_ms.h is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "stack-lockfree.h"
#if defined(RELAXATION_ANALYSIS)
	#include "relaxation_analysis_queue.h"
#endif

sval_t mstack_relaxed_find(mstack_t *set, skey_t key);
int mstack_relaxed_insert(skey_t key, sval_t val);
sval_t mstack_relaxed_delete();
//ad
extern __thread ssmem_allocator_t* alloc_segment;
typedef ALIGNED(CACHE_LINE_SIZE) struct index_struct
{
	mstack_node_t* node;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(mstack_node_t*)];
} index_t;
typedef ALIGNED(CACHE_LINE_SIZE) struct segment_struct segment_t;
struct segment_struct
{
	volatile index_t* indices;
	segment_t* next;
	long volatile remove;
	uint8_t padding[CACHE_LINE_SIZE - 16 - sizeof(long)];
};
typedef struct head_struct
{
	segment_t* segment;
	uint64_t version;
}head_t;

//int queue_relaxed_push_node(queue_node_t *node);
//sval_t queue_relaxed_delete_node(queue_node_t* node);
void get_push_slot();
void get_pop_slot();
unsigned long random_slot();
void try_create_segment();
int check_commit(mstack_node_t* new_node);

extern volatile head_t* head;
extern volatile segment_t* tail;
extern size_t num_threads;
extern int segment_size;

extern __thread ssmem_allocator_t* alloc;
extern __thread int thread_id;
extern __thread unsigned long push_slot;
extern __thread unsigned long pop_slot;
extern __thread unsigned long walk_count;
extern __thread segment_t* current_segment;
extern __thread head_t current_head;
extern __thread mstack_node_t* current_node;

extern __thread unsigned long my_push_cas_fail_count;
extern __thread unsigned long my_pop_cas_fail_count;
extern __thread unsigned long my_null_count;
extern __thread unsigned long my_hop_count;
extern __thread unsigned long my_slide_count;