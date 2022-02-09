#ifndef CONTROLLER_H
#define CONTROLLER_H

// This is just a way to modularize the elastic controller.
// Can only be included directly into the queue
#include "utils.h"
#include <stdint.h>
#include <stdlib.h>

// How much to increment count at contention
#define CONT_INC 75
// How much to decrement count when no contention
#define UNCONT_DEC 1
// At what absolute count to change the relaxation
#define CONT_TRESHOLD 5000
// How often to halve the count, preventing drift
#define ITER_TRESHOLD 40000000
#define DIFF 5

typedef struct {
  /// The current balance of the controller, incremented at contention,
  /// decremented when no contention.
  int16_t count;

  /// The number of iterations since we last regulated the controller. To reach
  /// steady state.
  int16_t iters;

  uint16_t this_max;
  int16_t votes;
} elastic_controller_t;

static inline int inc_controller(elastic_controller_t *cont) {
  cont->count += CONT_INC;
  cont->iters += 1;
  if (unlikely(cont->iters == ITER_TRESHOLD)) {
    cont->count = cont->count >> 1;
    cont->iters = 0;
  }
  if (unlikely(cont->count > CONT_TRESHOLD)) {
    cont->count = 0;
    return true;
  } else {
    return false;
  }
}

static inline int dec_controller(elastic_controller_t *cont) {
  cont->count -= UNCONT_DEC;
  cont->iters += 1;
  if (unlikely(cont->iters == ITER_TRESHOLD)) {
    cont->count = cont->count >> 1;
    cont->iters = 0;
  }
  if (unlikely(cont->count < -CONT_TRESHOLD)) {
    cont->count = 0;
    return true;
  } else {
    return false;
  }
}
static inline void inc_get_controller(elastic_controller_t *cont, DS_TYPE *set,
                                      lateral_node_t *thread_win) {
  // Since we only control width, the get side does not have that much say (they
  // could see if both width are equal, and then change)
  return;
  // if (unlikely(inc_controller(cont))) {
  //     // Increase relaxation
  //     update_width(set, thread_win.width << 1);
  // }
}

static inline void dec_get_controller(elastic_controller_t *cont, DS_TYPE *set,
                                      lateral_node_t *thread_win) {
  return;
  // if (unlikely(dec_controller(cont)) && thread_win.width > 1) {
  //     // Decrease relaxation
  //     update_width(set, thread_win.width >> 1);
  // }
}

// For now, put and get are ~identical
static inline void inc_put_controller(elastic_controller_t *cont, DS_TYPE *set,
                                      lateral_node_t *thread_win) {
  if (unlikely(inc_controller(cont)) && (thread_win->width << 1)) {
    // Increase relaxation
    if (cont->this_max != thread_win->max) {
      cont->votes = 0;
      cont->this_max = thread_win->max;
    }
    cont->votes += 1;

    if (cont->votes > 0) {
      update_width(set, thread_win->width + DIFF);
    }
  }
}

static inline void dec_put_controller(elastic_controller_t *cont, DS_TYPE *set,
                                      lateral_node_t *thread_win) {
  if (unlikely(dec_controller(cont)) && thread_win->width > 1) {
    // Decrease relaxation

    if (cont->this_max != thread_win->max) {
      cont->votes = 0;
      cont->this_max = thread_win->max;
    }
    cont->votes -= 1;

    if (cont->votes < 0 && thread_win->width > DIFF) {
      update_width(set, thread_win->width - DIFF);
    }
  }
}

// Now we don't have anywhere to initialize this
// Could just add it the the queue.h
// static inline void init_controller(elastic_controller_t* cont) {
//     cont->count = cont->iters = 0;
// }

#endif
