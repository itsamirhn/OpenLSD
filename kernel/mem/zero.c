#include <types.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/sched/task.h>

extern size_t nuser_tasks;

static void zero_page_thread(void *arg)
{
	struct page_info *page;

	(void)arg;
	for (;;) {
		if (nuser_tasks == 0)
			task_destroy(cur_task);
		asm volatile("cli" ::: "memory");
		while ((page = page_zero_pending()) != NULL)
			page_zero_complete(page);
		sched_yield();
	}
}

void zero_page_thread_init(void)
{
	task_create_kernel(zero_page_thread, NULL);
}
