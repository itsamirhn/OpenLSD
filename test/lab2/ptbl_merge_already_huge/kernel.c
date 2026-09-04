/*
 * Test: ptbl_merge with Already Huge Page
 *
 * This test verifies that ptbl_merge correctly handles the case where the
 * entry already points to a huge page. When an entry already has the PAGE_HUGE
 * flag set, ptbl_merge should not modify the entry or change the reference
 * count of the associated page.
 *
 * The test:
 * 1. Allocates a huge page and sets up an entry pointing to it with PAGE_HUGE
 * 2. Calls ptbl_merge with the huge page entry
 * 3. Verifies that the entry remains unchanged and the page reference count
 *    is unchanged
 */

#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_HUGE | PAGE_WRITE | PAGE_NO_EXEC | PAGE_USER)

static int run_test()
{
	struct page_info *huge_page;
	physaddr_t entry, original_entry;
	uint16_t original_ref;
	size_t nfree = count_total_free_pages();

	huge_page = page_alloc(ALLOC_HUGE | ALLOC_ZERO);
	assert(huge_page != NULL);
	assert(huge_page->pp_order == BUDDY_2M_PAGE);
	huge_page->pp_ref += 1;

	// Create a huge page ptbl_entry
	entry = page2pa(huge_page) | TEST_ENTRY_FLAGS;

	original_entry = entry;
	original_ref = huge_page->pp_ref;

	// Attempt to merge the already huge page
	assert(ptbl_merge(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Check that nothing changed
	assert(entry == original_entry);
	assert(huge_page->pp_ref == original_ref);
	assert((entry & PAGE_PRESENT) != 0);
	assert((entry & PAGE_HUGE) != 0);

	// Cleanup the state
	page_decref(huge_page);

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
