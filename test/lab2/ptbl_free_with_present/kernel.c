/*
 * Test: ptbl_free with Present Entry
 *
 * This test verifies that ptbl_free correctly handles the case where a page
 * table contains at least one present entry. When a page table has present
 * entries, ptbl_free should not free the page table and should leave the
 * entry unchanged.
 *
 * The test:
 * 1. Allocates a page table with one present entry
 * 2. Calls ptbl_free to attempt to free the page table
 * 3. Verifies that the entry remains unchanged and the page table is not freed
 */

#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

void test_ptbl_idx(struct page_info *ptbl_page, struct page_info *data_page, size_t idx)
{
	physaddr_t entry = page2pa(ptbl_page) | TEST_ENTRY_FLAGS;
	struct page_table *ptbl = page2kva(ptbl_page);
	physaddr_t original_entry = entry;
	uint16_t original_ref = ptbl_page->pp_ref;

	// Place the data page at idx
	ptbl->entries[idx] = page2pa(data_page) | TEST_ENTRY_FLAGS;

	// Check that free does not free this entry
	assert(ptbl_free(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);
	assert(ptbl_page->pp_ref == original_ref);
	assert(ptbl_page->pp_free == 0);

	// Check that the ptbl entry is unmodified
	assert(entry == original_entry);
	assert((entry & PAGE_PRESENT));
	assert(PAGE_ADDR(entry) == page2pa(ptbl_page));

	// Check that the data entry is still present and unchanged
	assert((ptbl->entries[idx] & PAGE_PRESENT) != 0);
	assert(PAGE_ADDR(ptbl->entries[idx]) == page2pa(data_page));

	// Clear out entry for the next test
	ptbl->entries[idx] = 0;
}

static int run_test()
{
	struct page_info *ptbl_page, *data_page;
	size_t nfree = count_total_free_pages();

	// Allocate some pages for the test
	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref += 1;

	data_page = page_alloc(ALLOC_ZERO);
	assert(data_page != NULL);
	data_page->pp_ref += 1;

	// Check freeing with a few interesting entries still present
	test_ptbl_idx(ptbl_page, data_page, 0);
	test_ptbl_idx(ptbl_page, data_page, PAGE_TABLE_ENTRIES - 1);
	test_ptbl_idx(ptbl_page, data_page, PAGE_TABLE_ENTRIES / 2);

	// Clean up state
	page_decref(data_page);
	page_decref(ptbl_page);

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
