#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *page;

	page = page_alloc(ALLOC_HUGE);
	assert(page != NULL);
	// To make sure it won't merge with higher size
	assert(page->pp_order == BUDDY_MAX_ORDER - 1);

	page_free(page);
	assert(page->pp_free == 1);

	cprintf("[TEST] Freeing the same page twice, this should panic\n");
	page_free(page);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
