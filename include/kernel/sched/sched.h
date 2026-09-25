#pragma once

#include<task.h>

void sched_init(void);
void sched_yield(void);
void sched_halt(void);
void sched_enqueue(struct task *task);
