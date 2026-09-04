/*
 * Test: ptbl_alloc with Unpresent Entry
 *
 * This test verifies that ptbl_alloc correctly allocates a new page table
 * when the entry is not present. When an entry does not have the PAGE_PRESENT
 * flag set, ptbl_alloc should allocate a new page, set up the entry with
 * appropriate flags, and establish a reference to the allocated page.
 *
 * The test:
 * 1. Starts with an unpresent entry (value 0)
 * 2. Calls ptbl_alloc to allocate a new page table
 * 3. Verifies that the entry is now present with correct flags
 * 4. Verifies that the allocated page has a reference count of 1
 */

#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define EXPECTED_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

static int run_test()
{
	struct page_info *ptbl_page;
	physaddr_t entry = 0;
	size_t nfree = count_total_free_pages();

	// Allocate a ptbl with providing an empty entry
	assert(ptbl_alloc(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Check that the created entry is sane
	assert((entry & PAGE_PRESENT) != 0);
	assert((entry & EXPECTED_ENTRY_FLAGS) == EXPECTED_ENTRY_FLAGS);

	// Check that the allocated page is sane
	ptbl_page = pa2page(PAGE_ADDR(entry));
	assert(ptbl_page != NULL);
	assert(ptbl_page->pp_ref == 1);

	// Clean up state
	page_decref(ptbl_page);
	assert(ptbl_page->pp_ref == 0);

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
