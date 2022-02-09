#ifndef LATERAL_STACK_H
#define LATERAL_STACK_H

#include <stdint.h>
#include "types.h"

// Forward declaration due to circular dependence (the three files all share logic, but are split up to make it easier to intuitevely separate)
struct file_descriptor;
typedef struct file_descriptor descriptor_t;


/* Type definitions */
typedef struct lateral_node
{
	struct lateral_node* next;
    row_t next_count;
	width_t width;
	
	uint8_t padding[CACHE_LINE_SIZE - sizeof(struct lateral_node*) - sizeof(uint32_t) - sizeof(uint8_t)];
} lateral_node_t;

typedef struct lateral_descriptor
{
	lateral_node_t* node;
	row_t count;
	uint32_t version; // Synced with the normal data stacks counter
} lateral_descriptor_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct lateral_block
{
	volatile lateral_descriptor_t descriptor; 
	uint8_t padding[CACHE_LINE_SIZE - sizeof(lateral_descriptor_t)]; 
} lateral_stack_t;


/* Interfaces */
void synchronize_lateral(lateral_stack_t* lateral, descriptor_t* substructures);
lateral_stack_t* create_lateral_stack(width_t width);

#endif
