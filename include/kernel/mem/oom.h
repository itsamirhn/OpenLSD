#pragma once

#include <types.h>
#include <task.h>

size_t task_footprint(struct task *task);
bool oom_kill(void);
