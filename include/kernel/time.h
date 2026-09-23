#pragma once

#include <time.h>

void time_init(void);
void time_now(struct timeval *tv);

uint64_t time_tsc_khz(void);
uint64_t time_tsc_base(void);
time_t time_epoch_base(void);
