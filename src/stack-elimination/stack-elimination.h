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

sval_t mstack_elimination_find(mstack_t *set, skey_t key);
int mstack_elimination_insert(mstack_t *set, skey_t key, sval_t val);
sval_t mstack_elimination_delete(mstack_t *set);
//ad
int mstack_elimination_push_node(mstack_t *set, mstack_node_t *node);
sval_t mstack_elimination_delete_node(mstack_node_t* node);
extern __thread unsigned long pop_slot;
unsigned long random_slot();
extern size_t num_threads;
extern __thread int thread_id; //global thread specific idetifier
extern __thread unsigned long my_push_cas_fail_count;
extern __thread unsigned long my_pop_cas_fail_count;
extern __thread unsigned long my_null_count;
extern __thread unsigned long my_hop_count;
extern __thread unsigned long my_slide_count;
typedef ALIGNED(CACHE_LINE_SIZE) struct exchanger
{
	mstack_node_t* node;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(mstack_node_t*)];
} exchanger_t;
typedef ALIGNED(CACHE_LINE_SIZE) struct wait_counter
{
	uint32_t count;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(uint32_t)];
} wait_counter_t;
extern volatile exchanger_t *elimination_array;
extern volatile wait_counter_t *wait_array;
extern volatile uint32_t push_wait;
extern volatile uint32_t pop_wait;