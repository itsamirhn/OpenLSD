
#include <types.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct list runq;

#define SCHED_TIME_BUDGET 10000000ULL

extern size_t nuser_tasks;

void sched_init(void)
{
	list_init(&runq);
}


/* Runs the next runnable task. */
void sched_yield(void)
{
	struct list *node;
	struct list *best_node = NULL;
	struct task *best_task = NULL;
	uint64_t elapsed = 0;

	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		elapsed = read_tsc() - cur_task->task_start_tsc;
		if (elapsed >= cur_task->task_budget){
			cur_task->task_budget = 0;
		}else{
			cur_task->task_budget -= elapsed;
		}
	}

	list_foreach(&runq, node) {
		struct task *task = container_of(node, struct task, task_node);
		if (!best_task || task->task_budget > best_task->task_budget) {
			best_task = task;
			best_node = node;
		}
	}

	if (best_task && cur_task && cur_task->task_budget == 0 && best_task->task_budget == 0) {
		cur_task->task_budget = SCHED_TIME_BUDGET;
		list_foreach(&runq, node){
			container_of(node, struct task, task_node)->task_budget = SCHED_TIME_BUDGET;
		}
		best_task = container_of(list_head(&runq), struct task, task_node);
		best_node = &best_task->task_node;
	}

	if (best_node) {
		list_del(best_node);
		best_task->task_start_tsc = read_tsc();
		return task_run(best_task);
	}
	
	if (cur_task && cur_task->task_status == TASK_RUNNING)
		return task_run(cur_task);

	cprintf("No runnable tasks!\n");
	halt_kernel();
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	halt_kernel();
}

void sched_enqueue(struct task *task) {
	task->task_status = TASK_RUNNABLE;
	list_add_tail(&runq, &task->task_node); 
}
