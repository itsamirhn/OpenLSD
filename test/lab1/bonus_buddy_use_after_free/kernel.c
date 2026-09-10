#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *huge, *again;
	unsigned char *va;

	// Leave exactly one order-9 block on the free list, so the block we free below is the one the next huge allocation must hand back
	while (count_free_pages(BUDDY_2M_PAGE) > 1) assert(page_alloc(ALLOC_HUGE) != NULL);

	huge = page_alloc(ALLOC_HUGE);
	assert(huge != NULL);
	assert(huge->pp_order == BUDDY_MAX_ORDER - 1);

	va = page2kva(huge);

	page_free(huge);
	assert(huge->pp_free == 1);

	va[0] = 0xAA;

	cprintf("[TEST] Reallocating a use-after-free page; this should panic\n");
	again = page_alloc(ALLOC_HUGE);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
