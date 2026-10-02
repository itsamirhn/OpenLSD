#pragma once

#include<task.h>

void sched_init(void);
void sched_yield(void);
void sched_halt(void);
void sched_enqueue(struct task *task);
void sched_dequeue(struct task *task);
void sched_sleep(uint64_t ns);
void sched_kick_from_bed(struct task *task);
