#include <lib.h>
#include <types.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct rb_tree runq;
struct rb_tree sleepq;

extern size_t nuser_tasks;

static uint64_t min_karma;

int rb_sleep_cmp(struct task *a, struct task *b) {
	if (a->task_wakeup_tsc < b->task_wakeup_tsc) return -1;
	if (a->task_wakeup_tsc > b->task_wakeup_tsc) return 1;
	return 0;
}

RB_DEFINE_INSERT_FUNC(struct task, rb_sleep_insert, rb_sleep_cmp, task_sleep_rb)

int rb_karma_cmp(struct task *a, struct task *b) {
	if (a->task_karma < b->task_karma) return -1;
	if (a->task_karma > b->task_karma) return 1;
	return 0;
}

RB_DEFINE_INSERT_FUNC(struct task, rb_runq_insert, rb_karma_cmp, task_karma_rb)

void sched_init(void)
{
	rb_init(&runq);
	rb_init(&sleepq);
}

void wakeup(void) {
	while (sleepq.root) {
		struct task *task = rb_first(&sleepq, struct task, task_sleep_rb);
		if (task->task_wakeup_tsc > read_tsc()) break;
		rb_remove(&sleepq, &task->task_sleep_rb);
		sched_enqueue(task);
	}
}

/* Runs the next runnable task. */
void sched_yield(void)
{
	#ifdef BONUS_SLEEP_TIME
	wakeup();
	#endif

	if (cur_task && cur_task->task_status == TASK_RUNNING)
		cur_task->task_karma += read_tsc() - cur_task->task_start_tsc;

	if (runq.root) {
		struct task *task = rb_first(&runq, struct task, task_karma_rb);
		rb_remove(&runq, &task->task_karma_rb);
		min_karma = MAX(min_karma, task->task_karma);
		task->task_start_tsc = read_tsc();
		return task_run(task);
	}
	
	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		cur_task->task_start_tsc = read_tsc();
		return task_run(cur_task);
	}

	#ifdef BONUS_SLEEP_TIME
	if (sleepq.root) {
		while (!runq.root) wakeup();
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
	task->task_karma = MAX(task->task_karma, min_karma);
	task->task_status = TASK_RUNNABLE;
	rb_node_init(&task->task_karma_rb);
	rb_runq_insert(&runq, task, NULL);
}

void sched_dequeue(struct task *task) {
	rb_remove(&runq, &task->task_karma_rb);
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
