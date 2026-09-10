#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *a;
	unsigned char *va;

	while (count_free_pages(BUDDY_4K_PAGE) > 0) assert(page_alloc(0) != NULL);

	a = page_alloc(0);
	assert(a != NULL);
	assert(count_free_pages(BUDDY_4K_PAGE) == 1);

	va = page2kva(a);
	va[PAGE_SIZE] = 0xAA;

	cprintf("[TEST] Accessing memory beyond the allocated page; this should panic\n");
	page_alloc(0);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
