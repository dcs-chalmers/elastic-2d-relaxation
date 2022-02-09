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
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "common.h"

#include <atomic_ops.h>
#include "lock_if.h"
#include "ssmem.h"
#include "utils.h"

/* ################################################################### *
	* Definition of macros: per data structure
* ################################################################### */

#define DS_CONTAINS(s,k,t)  set_contains(s)
#define DS_ADD(s,k,v)       increment(s)
#define DS_REMOVE(s)        decrement(s)
#define DS_SIZE(s)          counter_size(s)
#define DS_NEW(w)            create_counter(w)

#define DS_TYPE             counter_t
#define DS_NODE             index_t

/* Type definitions */
typedef ALIGNED(CACHE_LINE_SIZE) struct file_descriptor
{
	int64_t count;
	uint64_t version;
} descriptor_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct counter_node
{
	int64_t count;
	uint64_t version;
	uint8_t padding[CACHE_LINE_SIZE - (sizeof(int64_t) + sizeof(uint64_t))];
} index_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct counter
{
	index_t *array;
	uint64_t width;
	uint8_t padding[CACHE_LINE_SIZE - sizeof(index_t *)];
} counter_t;

/*Global variables*/


/*Thread local variables*/
extern __thread unsigned long* seeds;
extern __thread ssmem_allocator_t* alloc;
extern __thread unsigned long my_put_cas_fail_count;
extern __thread unsigned long my_get_cas_fail_count;
extern __thread unsigned long my_null_count;
extern __thread unsigned long my_hop_count;
extern __thread unsigned long my_slide_count;


/* Interfaces */
descriptor_t get_increment_index(counter_t *set);
descriptor_t get_decrement_index(counter_t *set);
unsigned long random_index(counter_t *set);
uint64_t increment(counter_t *set);
uint64_t decrement(counter_t *set);
counter_t* create_counter(uint64_t width);
size_t counter_size(counter_t *set);