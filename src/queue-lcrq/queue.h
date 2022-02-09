#ifndef QUEUE_H
#define QUEUE_H

#include "lcrq.h"

void queue_init(queue_t * q, int nprocs);
void queue_register(queue_t * q, handle_t * th, int id);
void queue_free(queue_t * q, handle_t * h);
void handle_free(handle_t *h);


/* INTERFACE FOR 2D TESTING FRAMEWORK */

#define DS_ADD(s,k,v)       enqueue_wrap(s.q, s.th, (void*) &k)
#define DS_REMOVE(s)        dequeue_wrap(s.q, s.th)
// #define DS_SIZE(s)          queue_size(s)
#define DS_NEW(q, n)        queue_init(q, n)

#define DS_TYPE queue_t

// Expose functions
int enqueue_wrap(queue_t *q, handle_t *th, void *v);
int dequeue_wrap(queue_t *q, handle_t *th);

typedef struct set_t {
  queue_t* q;
  handle_t* th;
} thread_set_t;

/* End of interface */

#endif /* end of include guard: QUEUE_H */
