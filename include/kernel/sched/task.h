
#pragma once

#include <task.h>
#include <cpu.h>

#define cur_task (this_cpu->cpu_task)

struct task *pid2task(pid_t pid, int check_perm);
void task_init(void);
struct task *task_alloc(pid_t ppid);
void task_create(uint8_t *binary, enum task_type type);
void task_free(struct task *task);
void task_destroy(struct task *task);
void task_pop_frame(struct int_frame *frame);
int task_exec(uint8_t *binary);
void task_run(struct task *task);
void assert_user_mem(struct task *task, void *va, size_t size, int flags);
