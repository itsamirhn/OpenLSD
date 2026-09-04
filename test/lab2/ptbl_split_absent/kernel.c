/*
 * Test: ptbl_split with Unpresent Entry
 *
 * This test verifies that ptbl_split correctly allocates a new page table
 * when the entry is not present. When an entry does not have the PAGE_PRESENT
 * flag set, ptbl_split should allocate a new page table and set up the entry
 * with appropriate flags.
 *
 * The test:
 * 1. Starts with an unpresent entry (value 0)
 * 2. Calls ptbl_split to allocate a new page table
 * 3. Verifies that the entry is now present with correct flags (no PAGE_HUGE)
 * 4. Verifies that the allocated page has a valid reference count
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
	physaddr_t entry = 0;
	size_t nfree = count_total_free_pages();

	// Attempt to split a page with an empty entry
	assert(ptbl_split(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);
	assert(entry != 0);
	assert((entry & PAGE_MASK) == TEST_ENTRY_FLAGS);

	// Check that the allocated entry is as expected
	page = pa2page(PAGE_ADDR(entry));
	assert(page != NULL);
	assert(page->pp_ref > 0 && page->pp_avail);
	assert(page->pp_order == BUDDY_4K_PAGE);

	// Free the page that was just allocated
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
