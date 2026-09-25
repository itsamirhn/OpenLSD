
#include <types.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct list runq;


extern size_t nuser_tasks;

void sched_init(void)
{
	list_init(&runq);
}


/* Runs the next runnable task. */
void sched_yield(void)
{
	if (!list_is_empty(&runq)) 
		return task_run(container_of(list_pop(&runq), struct task, task_node));
	
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
