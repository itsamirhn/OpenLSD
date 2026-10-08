#include <types.h>
#include <string.h>

#include <cpu.h>

#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/sched/task.h>

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

static void zero_page_thread(void *arg){
	struct page_info *page;

	for (;;) {
		asm volatile("cli" ::: "memory");
		big_spin_lock(&kernel_lock);
		while ((page = page_zero_pending()) != NULL)
			page_zero_complete(page);
		// Sleep until page_free() queues a new page on this CPU
		cur_task->task_status = TASK_NOT_RUNNABLE;
		sched_yield();
	}
}

// Wakes this CPU's zero thread after page_free() queued a page for it
void zero_page_thread_wake(void) {
	struct task *task = this_cpu->cpu_zero_task;
	if (task && task->task_status == TASK_NOT_RUNNABLE) sched_enqueue(task);
}

// One zero thread per CPU, as the pending lists are per CPU
void zero_page_thread_init(void)
{
	for (size_t i = 0; i < ncpus; i++) 
		cpus[i].cpu_zero_task = kthread_create(zero_page_thread, NULL, 1ULL << i);
}
