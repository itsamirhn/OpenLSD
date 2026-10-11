#include <assert.h>
#include <cpu.h>
#include <list.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/sched/hotplug.h>
#include <kernel/sched/task.h>
#include <kernel/test/test.h>

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

#define CORE   2
#define NPAGES 8

// The per-CPU lists may only be touched with interrupts off
static void enter(void)
{
	asm volatile("cli" ::: "memory");
	big_spin_lock(&kernel_lock);
}

static void leave(void)
{
	big_spin_unlock(&kernel_lock);
	asm volatile("sti" ::: "memory");
}

static bool in_list(struct list *head, struct page_info *page)
{
	struct list *node;

	list_foreach(head, node){
		if (container_of(node, struct page_info, pp_node) == page) return true;
	}
	return false;
}

// Wait, with interrupts on, until this CPU's zero thread cached the page
static void wait_cached(struct page_info *page)
{
	unsigned long timeout = 1000000000;
	bool cached = false;

	leave();
	while (!cached && timeout-- != 0) {
		asm volatile("pause");
		enter();
		cached = in_list(&this_cpu->cpu_page_cache, page);
		leave();
	}
	enter();
	assert(cached);
}

static void off_worker(void *arg)
{
	unsigned long timeout = 1000000000;

	enter();
	assert(sys_core_disable(CORE) == 0);
	leave();

	while (cpus[CORE].cpu_status != CPU_HALTED && timeout-- != 0) asm volatile("pause");

	enter();
	assert(cpus[CORE].cpu_status == CPU_HALTED);

	// Nobody can allocate from a parked core's cache, so it must be empty
	assert(cpus[CORE].cpu_page_cache_count == 0);
	assert(list_is_empty(&cpus[CORE].cpu_page_cache));

	cprintf("[TEST] CPU %d gave its cached pages back before powering off\n", CORE);
	halt_kernel();
}

static void fill_worker(void *arg)
{
	struct page_info *pages[NPAGES];

	enter();

	// Freed pages go through the zero thread into this CPU's cache
	for (int i = 0; i < NPAGES; i++) {
		pages[i] = page_alloc(0);
		assert(pages[i] != NULL);
	}
	for (int i = 0; i < NPAGES; i++) page_free(pages[i]);
	for (int i = 0; i < NPAGES; i++) wait_cached(pages[i]);
	assert(cpus[CORE].cpu_page_cache_count >= NPAGES);

	cprintf("[TEST] Filled the page cache of CPU %d\n", CORE);
	kthread_create(off_worker, NULL, 1ULL << 0);

	cur_task->task_status = TASK_NOT_RUNNABLE;
	sched_yield();
}

static int run_test(struct probe_frame *frame)
{
	kthread_create(fill_worker, NULL, 1ULL << CORE);
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_pop_frame,
	.should_continue = true,
	.checksum = __checksum__,
};
