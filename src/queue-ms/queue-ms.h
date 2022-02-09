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

/* ################################################################### *
 * Definition of macros: per data structure
 * ################################################################### */

#define DS_CONTAINS(s,k,t)  queue_contains(s, k)
#define DS_ADD(s,k,t)       queue_add(s, k, t)
#define DS_REMOVE(s)        queue_remove(s)
#define DS_SIZE(s)          queue_size(s)
#define DS_NEW()            queue_new()

#define DS_TYPE             queue_t
#define DS_NODE             queue_node_t

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

#include "queue-lockfree.h"

sval_t queue_ms_find(queue_t *set, skey_t key);
int queue_ms_insert(queue_t *set, skey_t key, sval_t val);
sval_t queue_ms_delete(queue_t *set);
