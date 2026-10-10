#include <assert.h>
#include <cpu.h>
#include <list.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/sched/task.h>
#include <kernel/test/test.h>

#if defined(BONUS_MULTI_CORE_FRAME_ALLOCATOR) || defined(BONUS_LAB6)

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#else
extern struct spinlock buddy_lock;
#endif
extern struct list buddy_free_list[];

static struct page_info *page_a, *page_b;
static volatile bool page_a_cached;

// The per-CPU lists should only be touched with interrupts off
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

static void sleep_forever(void)
{
	cur_task->task_status = TASK_NOT_RUNNABLE;
	sched_yield();
}

static void merge_worker(void *arg)
{
	unsigned long timeout = 1000000000;

	while (!page_a_cached && timeout-- != 0) asm volatile("pause");

	assert(page_a_cached);

	enter();
	assert(in_list(&cpus[1].cpu_page_cache, page_a));

	// Free B on CPU 0 and give CPU 0's cache back, which runs buddy_merge(B)
	page_free(page_b);
	wait_cached(page_b);
	page_cache_flush();

	// A is free but cached on CPU 1, so B must stay an order-0 block on its own
	assert(in_list(&cpus[1].cpu_page_cache, page_a));
	assert(page_a->pp_order == BUDDY_4K_PAGE && !page_a->pp_free);
	assert(page_b->pp_order == BUDDY_4K_PAGE && page_b->pp_free);
	assert(in_list(buddy_free_list + BUDDY_4K_PAGE, page_b));

	cprintf("[TEST] buddy B was not merged with page A cached on CPU 1\n");
	halt_kernel();
}

static void cache_worker(void *arg)
{
	struct page_info *block;

	(void)arg;
	enter();

	// Split an 8k block by hand into the buddies A and B
	fine_spin_lock(&buddy_lock);
	block = buddy_find(1);
	fine_spin_unlock(&buddy_lock);
	assert(block != NULL);

	page_a = block;
	page_b = block + 1;
	page_a->pp_order = BUDDY_4K_PAGE;
	page_b->pp_order = BUDDY_4K_PAGE;

	// Free A on CPU 1, so the zero thread puts it in CPU 1's cache
	page_free(page_a);
	wait_cached(page_a);
	cprintf("[TEST] page A is cached on CPU 1\n");

	// Nothing may allocate on CPU 1 from here on, or it could pop A again
	page_a_cached = true;
	sleep_forever();
}

static int run_test(struct probe_frame *frame)
{
	kthread_create(merge_worker, NULL, 1ULL << 0);
	kthread_create(cache_worker, NULL, 1ULL << 1);
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_pop_frame,
	.should_continue = true,
	.checksum = __checksum__,
};

#endif
