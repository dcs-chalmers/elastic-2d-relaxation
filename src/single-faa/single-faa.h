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

#define DS_ADD(faa_ptr,_1,_2)       inc_faa(faa_ptr)
#define DS_REMOVE(faa_ptr)        dec_faa(faa_ptr)
#define DS_SIZE(faa_ptr)          read_faa(faa_ptr)
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

extern __thread unsigned long my_put_faa_fail_count;
extern __thread unsigned long my_get_faa_fail_count;

static int64_t inc_faa(_Atomic int64_t* ptr) {
    return atomic_fetch_add(ptr, 1);
}

static int64_t dec_faa(_Atomic int64_t* ptr) {
    return atomic_fetch_add(ptr, -1);
}

static int64_t read_faa(_Atomic int64_t* ptr) {
    if (ptr != NULL) {
        return atomic_load(ptr);
    }
    return 0; // or some error value
}
