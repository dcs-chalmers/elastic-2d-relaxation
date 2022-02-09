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
#include "utils.h"
#include <stdatomic.h>

 /* ################################################################### *
	* Definition of macros: per data structure
* ################################################################### */

#define DS_ADD(cas_ptr,_1,_2)       inc_cas(cas_ptr)
#define DS_REMOVE(cas_ptr)        dec_cas(cas_ptr)
#define DS_SIZE(cas_ptr)          read_cas(cas_ptr)
#define DS_NEW()         create()

#define DS_TYPE             _Atomic int64_t
#define DS_NODE             int64_t

static _Atomic int64_t* create() {
    _Atomic int64_t* ptr = malloc(sizeof(_Atomic int64_t));
    if (ptr != NULL) {
        atomic_init(ptr, 0);
    }
    atomic_store(ptr, 0);
    return ptr;
}

extern __thread unsigned long my_put_cas_fail_count;
extern __thread unsigned long my_get_cas_fail_count;

static int64_t inc_cas(_Atomic int64_t* ptr) {
	int64_t oldValue, newValue;
	oldValue = atomic_load_explicit(ptr, __ATOMIC_RELAXED);
    newValue = oldValue + 1;
    while (!atomic_compare_exchange_weak(ptr, &oldValue, newValue)) {
		my_put_cas_fail_count += 1;
	    newValue = oldValue + 1;
    }
    return oldValue;
}

static int64_t dec_cas(_Atomic int64_t* ptr) {
	int64_t oldValue, newValue;
    oldValue = atomic_load_explicit(ptr, __ATOMIC_RELAXED);
    newValue = oldValue - 1;
    while (!atomic_compare_exchange_weak(ptr, &oldValue, newValue)) {
		my_get_cas_fail_count += 1;
	    newValue = oldValue - 1;
    }
    return newValue;
}

static int64_t read_cas(_Atomic int64_t* ptr) {
    if (ptr != NULL) {
        return atomic_load(ptr);
    }
    return 0; // or some error value
}
