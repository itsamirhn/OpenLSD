#include <types.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/sched/task.h>

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

extern size_t nuser_tasks;

static void zero_page_thread(void *arg)
{
	struct page_info *page;

	(void)arg;
	for (;;) {
		asm volatile("cli" ::: "memory");
		big_spin_lock(&kernel_lock);
		while ((page = page_zero_pending()) != NULL)
			page_zero_complete(page);
		if (nuser_tasks == 0)
			task_destroy(cur_task);
		sched_yield();
	}
}

void zero_page_thread_init(void)
{
	kthread_create(zero_page_thread, NULL);
}
