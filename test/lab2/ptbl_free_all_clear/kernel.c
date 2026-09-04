/*
 * Test: ptbl_free with All Clear Entries
 *
 * This test verifies that ptbl_free correctly frees a page table when all
 * entries in the table are clear (not present). When a page table has no
 * present entries, ptbl_free should free the page table page and clear the
 * entry.
 *
 * The test:
 * 1. Allocates a page table with all entries clear
 * 2. Calls ptbl_free to free the page table
 * 3. Verifies that the entry is cleared and the page is freed
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
	struct page_info *ptbl_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate a page for the ptbl
	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref += 1;

	// Create the entry for the ptbl
	entry = page2pa(ptbl_page) | TEST_ENTRY_FLAGS;
	ptbl = page2kva(ptbl_page);

	// Check that ptbl_free works appropriately
	assert(ptbl_free(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);
	assert(entry == 0);
	assert(ptbl_page->pp_ref == 0);
	assert(ptbl_page->pp_free == 1);

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
