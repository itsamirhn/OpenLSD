/*
 * Test: ptbl_split with Non-Huge Page
 *
 * This test verifies that ptbl_split correctly handles the case where the
 * entry does not point to a huge page. When an entry does not have the
 * PAGE_HUGE flag set, ptbl_split should not modify the entry or change the
 * reference count of the associated page.
 *
 * The test:
 * 1. Allocates a regular 4K page and sets up an entry pointing to it without
 *    PAGE_HUGE
 * 2. Calls ptbl_split with the non-huge entry
 * 3. Verifies that the entry remains unchanged and the page reference count
 *    is unchanged
 */

#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

static int run_test()
{
	struct page_info *page;
	struct page_info original_page;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate our single page ptbl
	page = page_alloc(ALLOC_ZERO);
	assert(page != NULL);
	page->pp_ref += 1;

	entry = page2pa(page) | TEST_ENTRY_FLAGS;
	original_page = *page;

	// Attempt to split the non-huge page
	assert(ptbl_split(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Check that we get the same entry back
	assert(pa2page(PAGE_ADDR(entry)) == page);
	assert((entry & PAGE_MASK) == TEST_ENTRY_FLAGS);
	assert(page->pp_ref == original_page.pp_ref);

	// Free allocated pages
	page_decref(page);

	// Check for memory leaks
	assert(nfree == count_total_free_pages());

	return __checksum__;
}

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = page_init_ext,
    .should_continue = false,
    .checksum = __checksum__,
};
