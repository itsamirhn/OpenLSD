#include <lib.h>
#include <types.h>
#include <cpu.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/sched/hotplug.h>

#define SCHED_BALANCE_PICKS 10

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

struct rb_tree runq;
struct rb_tree sleepq;

#ifndef USE_BIG_KERNEL_LOCK
struct spinlock runq_lock = {
	.rank = RANK_SCHED,
#ifdef DEBUG_SPINLOCK
	.name = "runq_lock",
#endif
};
#endif

extern size_t nuser_tasks;

int rb_sleep_cmp(struct task *a, struct task *b) {
	if (a->task_wakeup_tsc < b->task_wakeup_tsc) return -1;
	if (a->task_wakeup_tsc > b->task_wakeup_tsc) return 1;
	return 0;
}

RB_DEFINE_INSERT_FUNC(struct task, rb_sleep_insert, rb_sleep_cmp, task_sched_rb)

int rb_karma_cmp(struct task *a, struct task *b) {
	if (a->task_karma < b->task_karma) return -1;
	if (a->task_karma > b->task_karma) return 1;
	return 0;
}

RB_DEFINE_INSERT_FUNC(struct task, rb_runq_insert, rb_karma_cmp, task_sched_rb)

static void runq_insert(struct rb_tree *tree, struct task *task) {
	struct task *first = rb_first(tree, struct task, task_sched_rb);
	task->task_karma = MAX(task->task_karma, first ? first->task_karma : 0);
	task->task_status = TASK_RUNNABLE;
	rb_node_init(&task->task_sched_rb);
	rb_runq_insert(tree, task, NULL);
}

#ifdef BONUS_CORE_HOTPLUGGING
#define runnable_by_me(task) (core_task_affinity(task) & (1ULL << lapic_cpunum()))
#else
#define runnable_by_me(task) ((task)->task_affinity & (1ULL << lapic_cpunum()))
#endif

static void sched_enqueue_local(struct task *task)
{
	runq_insert(&this_cpu->runq, task);
}

void sched_init(void)
{
	rb_init(&runq);
	rb_init(&sleepq);
	sched_init_mp();
}

void sched_init_mp(void)
{
	rb_init(&this_cpu->runq);
	this_cpu->runq_picks = 0;
}

void wakeup(void) {
	while (sleepq.root) {
		struct task *task = rb_first(&sleepq, struct task, task_sched_rb);
		if (task->task_wakeup_tsc > read_tsc()) break;
		rb_remove(&sleepq, &task->task_sched_rb);
		sched_enqueue_local(task);
	}
}

static void sched_balance(void) {
	struct rb_tree *local = &this_cpu->runq;
	struct task *task, *pulled = NULL;

	if (!fine_spin_trylock(&runq_lock)) return;

	// Move one min from global to local because having one job is enough
	for (int i = 0; i < runq.size; i++) {
		task = container_of(rb_index_element(&runq, i), struct task, task_sched_rb);
		if (!runnable_by_me(task)) continue;
		rb_remove(&runq, &task->task_sched_rb);
		runq_insert(local, task);
		pulled = task;
		break;
	}

	// Move all max from local to global because they can do more while I'm busy
	while (local->size > runq.size + 1) {
		task = rb_last(local, struct task, task_sched_rb);
		// Never push back the task we just pulled, or it can starve on one CPU
		if (task == pulled)
			task = container_of(rb_index_element(local, local->size - 2), struct task, task_sched_rb);
		rb_remove(local, &task->task_sched_rb);
		runq_insert(&runq, task);
	}

#ifdef BONUS_CORE_HOTPLUGGING
	core_auto_wake();
#endif

	fine_spin_unlock(&runq_lock);
}

#ifdef BONUS_CORE_HOTPLUGGING
 // CPU should power off (idle or syscall) and now needs to move its tasks to the global run queue and sleep
 // does not return
static void sched_power_off(void){
	struct cpuinfo *cpu = this_cpu;
	struct task *task;

	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		cur_task->task_karma += read_tsc() - cur_task->task_start_tsc;
		sched_enqueue(cur_task);
	}
	cur_task = NULL;
	load_pml4(PADDR(kernel_pml4));

	while (cpu->runq.root) {
		task = rb_first(&cpu->runq, struct task, task_sched_rb);
		rb_remove(&cpu->runq, &task->task_sched_rb);
		sched_enqueue(task);
	}

	core_park();
}
#endif

/* Runs the next runnable task. */
void sched_yield(void)
{
	struct cpuinfo *cpu = this_cpu;

	#ifdef BONUS_SLEEP_TIME
	wakeup();
	#endif

	#ifdef BONUS_CORE_HOTPLUGGING
	if (cpu->cpu_off) sched_power_off();
	#endif

	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		cur_task->task_karma += read_tsc() - cur_task->task_start_tsc;
		if (!runnable_by_me(cur_task)) {
		sched_enqueue(cur_task);
		cur_task = NULL;
		load_pml4(PADDR(kernel_pml4));
		}
	}

	if (++cpu->runq_picks % SCHED_BALANCE_PICKS == 0 || !cpu->runq.root)
		sched_balance();

	while (cpu->runq.root) {
		struct task *task = rb_first(&cpu->runq, struct task, task_sched_rb);
		rb_remove(&cpu->runq, &task->task_sched_rb);
		if (!runnable_by_me(task)) {
			sched_enqueue(task);
			continue;
		}
		if (cur_task && cur_task->task_status == TASK_RUNNING)
			sched_enqueue_local(cur_task);
		task->task_start_tsc = read_tsc();
		return task_run(task);
	}

	if (cur_task && cur_task->task_status == TASK_RUNNING) {
		cur_task->task_start_tsc = read_tsc();
		return task_run(cur_task);
	}

	cur_task = NULL;
#ifdef BONUS_CORE_HOTPLUGGING
	uint64_t idle_start = read_tsc();
	cpu->cpu_idle = true;
#endif
	while (!cpu->runq.root && (nuser_tasks > 0 || cpu != boot_cpu)) {
		big_spin_unlock(&kernel_lock);
		asm volatile("pause" ::: "memory");
		big_spin_lock(&kernel_lock);
		if (runq.size) sched_balance();
	#ifdef BONUS_CORE_HOTPLUGGING
		if (core_should_power_off(&idle_start)) sched_power_off();
	#endif
	}
#ifdef BONUS_CORE_HOTPLUGGING
	cpu->cpu_idle = false;
#endif

	if (!cpu->runq.root) sched_halt();

	return sched_yield();
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	cprintf("No runnable tasks!\n");
	halt_kernel();
}

void sched_enqueue(struct task *task) {
	fine_spin_lock(&runq_lock);
	runq_insert(&runq, task);
#ifdef BONUS_CORE_HOTPLUGGING
	core_auto_wake();
#endif
	fine_spin_unlock(&runq_lock);
}

void sched_dequeue(struct task *task) {
	struct rb_node *top = &task->task_sched_rb;

	fine_spin_lock(&runq_lock);
	while (top->parent) top = top->parent;
	if (top == runq.root)
		rb_remove(&runq, &task->task_sched_rb);
	else if (top == this_cpu->runq.root)
		rb_remove(&this_cpu->runq, &task->task_sched_rb);
	fine_spin_unlock(&runq_lock);
}

void sched_sleep(uint64_t ns) {
	struct task *task = cur_task;
	task->task_status = TASK_SLEEPING;
	task->task_wakeup_tsc = read_tsc() + ns * time_tsc_khz() / NSEC_PER_MSEC;
	task->task_frame.rax = 0; // Sleep syscall return value after wakeup
	task->task_karma += read_tsc() - task->task_start_tsc;
	cur_task = NULL;
	load_pml4(PADDR(kernel_pml4));
	rb_node_init(&task->task_sched_rb);
	rb_sleep_insert(&sleepq, task, NULL);
	sched_yield();
}

void sched_kick_from_bed(struct task *task) {
	rb_remove(&sleepq, &task->task_sched_rb);
}
