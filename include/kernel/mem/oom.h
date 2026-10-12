#pragma once

#include <types.h>
#include <task.h>

size_t task_rss(struct task *task);
bool oom_kill(void);
