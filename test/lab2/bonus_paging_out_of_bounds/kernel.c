#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

extern void halt_kernel();

#define TEST_VA ((uintptr_t)0x40000000)

static int run_test() {
	struct paging_info info = {0};
	struct page_table *pml4 = kernel_pml4;
	struct page_info *page = page_alloc(ALLOC_ZERO);
	volatile unsigned char *mapped = (volatile unsigned char *)TEST_VA;

	assert(page != NULL);
	assert(setup_page_tables(&pml4, TEST_VA, PTE, &info) == 0);
	assert(page_insert_guarded(pml4, page, (void *)TEST_VA,
		PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE) == 0);

	mapped[PAGE_SIZE - 1] = 0xA5;
	assert(mapped[PAGE_SIZE - 1] == 0xA5);
	
	// these page_inserts should fail because of the guard pages
	assert(page_insert_guarded(pml4, page, (void *)TEST_VA + PAGE_SIZE,
		 PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC) < 0);
	assert(page_insert_guarded(pml4, page, (void *)TEST_VA - PAGE_SIZE,
		 PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC) < 0);
	
	struct page_info *page_huge = page_alloc(ALLOC_ZERO | ALLOC_HUGE);
	assert(page_insert_guarded(pml4, page_huge, (void *)TEST_VA - HPAGE_SIZE,
		 PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE) < 0);

	cprintf("[TEST] Accessing the unmapped guard page; this should panic\n");
	*((volatile unsigned char *)(TEST_VA + PAGE_SIZE)) = 0x5A;
	cprintf("[TEST] This did not panic!\n");


	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
