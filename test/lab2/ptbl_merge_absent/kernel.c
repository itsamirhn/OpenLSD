/*
 * Test: ptbl_merge with Not Present Entry
 *
 * This test verifies that ptbl_merge correctly handles the case where not all
 * entries in the page table are present. When some entries are not present,
 * ptbl_merge should not merge the entries and should leave the entry unchanged.
 *
 * The test:
 * 1. Allocates a page table with most entries present, but one entry missing
 * 2. Calls ptbl_merge to attempt to merge the entries
 * 3. Verifies that the entry remains unchanged (merge fails due to
 *    missing entry)
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

void test_missing_idx(physaddr_t *ptbl_entry, struct page_table *ptbl, size_t idx)
{
	physaddr_t original_entry = *ptbl_entry;
	physaddr_t missing_entry = ptbl->entries[idx];
	struct page_table original_ptbl;

	// Create a missing entry
	ptbl->entries[idx] = 0;

	// Remember this configuration
	original_ptbl = *ptbl;

	// Attempt to merge the table with a missing entry
	assert(ptbl_merge(ptbl_entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Check that nothing changed
	assert(*ptbl_entry == original_entry);
	assert(memcmp(ptbl, &original_ptbl, sizeof(struct page_table)) == 0);

	// Restore the page table
	ptbl->entries[idx] = missing_entry;
}

static int run_test()
{
	struct page_info *ptbl_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate and create our ptbl
	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref = 1;

	entry = page2pa(ptbl_page) | TEST_ENTRY_FLAGS;
	ptbl = page2kva(ptbl_page);

	// Allocate all the pages we will need
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		struct page_info *data_page = page_alloc(ALLOC_ZERO);
		assert(data_page != NULL);
		data_page->pp_ref = 1;
		ptbl->entries[i] = page2pa(data_page) | TEST_ENTRY_FLAGS;
	}

	// Try a few interesting indicies
	test_missing_idx(&entry, ptbl, 0);
	test_missing_idx(&entry, ptbl, PAGE_TABLE_ENTRIES - 1);
	test_missing_idx(&entry, ptbl, PAGE_TABLE_ENTRIES / 2);

	// Free allocated state
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		page_decref(pa2page(PAGE_ADDR(ptbl->entries[i])));

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
