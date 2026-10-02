#include <lib.h>
#include <types.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct list runq;
struct rb_tree sleepq;

extern size_t nuser_tasks;

int rb_sleep_cmp(struct task *a, struct task *b) {
	if (a->task_wakeup_tsc < b->task_wakeup_tsc) return -1;
	if (a->task_wakeup_tsc > b->task_wakeup_tsc) return 1;
	return 0;
}

RB_DEFINE_INSERT_FUNC(struct task, rb_sleep_insert, rb_sleep_cmp, task_sleep_rb)

void sched_init(void)
{
	list_init(&runq);
	rb_init(&sleepq);
}

void wakeup(void) {
	while (sleepq.root) {
		struct rb_node *first = sleepq.root;
		while (first->left) first = first->left;
		struct task *task = container_of(first, struct task, task_sleep_rb);
		if (task->task_wakeup_tsc > read_tsc()) break;
		rb_remove(&sleepq, first);
		sched_enqueue(task);
	}
}

/* Runs the next runnable task. */
void sched_yield(void)
{
	struct list *node;
	struct task *best_task = NULL;

	#ifdef BONUS_SLEEP_TIME
	wakeup();
	#endif

	if (cur_task && cur_task->task_status == TASK_RUNNING)
		cur_task->task_karma += read_tsc() - cur_task->task_start_tsc;

	list_foreach(&runq, node) {
		struct task *task = container_of(node, struct task, task_node);
		if (!best_task || task->task_karma < best_task->task_karma) best_task = task;
	}

	if (best_task) {
		list_del(&best_task->task_node);
		best_task->task_start_tsc = read_tsc();
		return task_run(best_task);
	}
	
	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		cur_task->task_start_tsc = read_tsc();
		return task_run(cur_task);
	}

	#ifdef BONUS_SLEEP_TIME
	if (sleepq.root) {
		while(list_is_empty(&runq)) wakeup();
		return sched_yield();
	}
	#endif

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

void sched_sleep(uint64_t ns) {
	struct task *task = cur_task;
	task->task_status = TASK_SLEEPING;
	task->task_wakeup_tsc = read_tsc() + ns * time_tsc_khz() / NSEC_PER_MSEC;
	task->task_frame.rax = 0; // Sleep syscall return value after wakeup
	rb_node_init(&task->task_sleep_rb);
	rb_sleep_insert(&sleepq, task, NULL);
	cur_task = NULL;
	sched_yield();
}

void sched_kick_from_bed(struct task *task) {
	rb_remove(&sleepq, &task->task_sleep_rb);
}
