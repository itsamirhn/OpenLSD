#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *bogus;

	bogus = pages + npages;

	cprintf("[TEST] Freeing a pointer outside the pages array; this should panic\n");
	page_free(bogus);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
