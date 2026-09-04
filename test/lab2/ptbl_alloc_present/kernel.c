/*
 * Test: ptbl_alloc with Present Entry
 *
 * This test verifies that ptbl_alloc correctly handles the case where the entry
 * is already present. When an entry already has the PAGE_PRESENT flag set,
 * ptbl_alloc should not modify the entry or change the reference count of the
 * associated page.
 *
 * The test:
 * 1. Allocates a page and sets up a present entry pointing to it
 * 2. Calls ptbl_alloc with the present entry
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
	physaddr_t entry;
	physaddr_t original_entry;
	uint16_t original_ref;
	size_t nfree = count_total_free_pages();

	page = page_alloc(ALLOC_ZERO);
	assert(page != NULL);
	page->pp_ref += 1;

	// Make an entry ourselves
	entry = page2pa(page) | TEST_ENTRY_FLAGS;
	original_entry = entry;
	original_ref = page->pp_ref;

	// Check that ptbl_alloc is a no-op in this case, as entry is already allocated
	assert(ptbl_alloc(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Check that our entry remains unchanged
	assert((entry & PAGE_PRESENT) != 0);
	assert(entry == original_entry);
	assert(page->pp_ref == original_ref);

	// Cleanup used state
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
