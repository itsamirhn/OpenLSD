#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *page, *bogus;

	page = page_alloc(0);
	assert(page != NULL);

	bogus = (struct page_info *)((char *)page + 8);

	assert(pages <= bogus && bogus < pages + npages);

	cprintf("[TEST] Freeing a misaligned page_info pointer; this should panic\n");
	page_free(bogus);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
