#ifndef RELAXATION_ANALYSIS_QUEUE_H
#define RELAXATION_ANALYSIS_QUEUE_H

#include "lock_if.h"
#include "common.h"


typedef struct linear_node_t linear_node_t;
typedef struct linear_list_t linear_list_t;

struct linear_list_t
{
  uint64_t size;
	linear_node_t* head;
  linear_node_t* tail;
};

struct linear_node_t
{

	sval_t val;
	linear_node_t* next;

};


extern ptlock_t relaxation_list_lock;
extern linear_list_t linear_clone;
extern linear_list_t error_dists;



void add_linear(sval_t val, int end);
void remove_linear(sval_t val);

void lock_relaxation_lists();
void unlock_relaxation_lists();

uint64_t gen_relaxation_count();

void init_relaxation_analysis();

void print_relaxation_measurements();

#endif
