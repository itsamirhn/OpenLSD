#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *head, *mid;

	head = buddy_find(2);
	assert(head != NULL);

	for (mid = head + 1; mid < head + (1 << 2); mid++) {
		assert(mid->pp_free == 0);
	}
	mid--;

	cprintf("[TEST] Freeing a pointer that is not a block header; this should panic\n");
	page_free(mid);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
