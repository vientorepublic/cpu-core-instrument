#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <stdatomic.h>

#define MAX_CORES 64

typedef struct {
    int num_cores;
    float usage[MAX_CORES];
} CoreUsageFrame;

typedef struct {
    atomic_uint_fast64_t seq;
    CoreUsageFrame frames[2];
} SharedCoreState;

#endif
