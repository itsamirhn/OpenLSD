#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

extern void halt_kernel();


#define TEST_VA ((uintptr_t)0x44000000)
#define BOGUS 16

static int run_test() {
	struct paging_info info = {0};
	struct page_table *pml4 = kernel_pml4;
	struct page_info *page = page_alloc(ALLOC_ZERO);

	assert(page != NULL);
	assert(setup_page_tables(&pml4, TEST_VA, PTE, &info) == 0);

	
	cprintf("[TEST] Removing an misaligned virtual address; this should panic\n");
	page_remove(pml4, (void *)(TEST_VA + BOGUS));

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
