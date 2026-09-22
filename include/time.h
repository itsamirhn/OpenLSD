#pragma once

#include <types.h>

typedef int64_t time_t;
typedef int64_t suseconds_t;

struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};
