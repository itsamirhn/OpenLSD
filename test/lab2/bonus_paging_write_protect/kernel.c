#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

extern void halt_kernel();

#define TEST_VA ((uintptr_t)0x42000000)

static int run_test() {
	struct paging_info info = {0};
	struct page_table *pml4 = kernel_pml4;
	struct page_info *page = page_alloc(ALLOC_ZERO);
	volatile unsigned char *read_only = (volatile unsigned char *)TEST_VA;

	assert(page != NULL);
	assert(setup_page_tables(&pml4, TEST_VA, PTE, &info) == 0);
	assert(page_insert(pml4, page, (void *)TEST_VA,
		PAGE_PRESENT | PAGE_NO_EXEC) == 0);
	assert(page_lookup(pml4, (void *)TEST_VA, NULL) == page);

	cprintf("[TEST] Writing through a read-only mapping; this should panic\n");
	read_only[0] = 0xA5;
	cprintf("[TEST] This did not panic!\n");


	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
