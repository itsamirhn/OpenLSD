#include <assert.h>
#include <cpu.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/test/test.h>

static void zero_test_worker(void *arg)
{

	volatile unsigned char *va;
	unsigned long timeout = 100000000;

	struct page_info *page = page_alloc(ALLOC_ZERO);
	assert(page != NULL);
	va = page2kva(page);
	memset((void *)va, 'A', PAGE_SIZE);
	cprintf("[TEST] page has char %c\n", va[0]);

	page_free(page);

	// Wait for the page to be zeroed by the zero page thread
	while (va[0] != 0 && timeout-- != 0){
		asm volatile("pause");
	}

	for (size_t i = 0; i < PAGE_SIZE; i++){
		assert(va[i] == 0);
	}

	cprintf("[TEST] page has zero %d\n", va[0]);
	halt_kernel();
}

static int run_test(struct probe_frame *frame)
{
	(void)frame;
	kthread_create(zero_test_worker, NULL, 1ULL << (this_cpu - cpus));
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_pop_frame,
	.should_continue = true,
	.checksum = __checksum__,
};
