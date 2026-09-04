/*
 * Test: ptbl_merge with Mismatched Flags
 *
 * This test verifies that ptbl_merge correctly handles the case where entries
 * in the page table have mismatched flags. When entries have different flags
 * (e.g., one entry has PAGE_NO_EXEC while others don't), ptbl_merge should
 * not merge the entries and should leave the entry unchanged.
 *
 * The test:
 * 1. Allocates a page table with 512 present entries, where one entry has
 *    different flags than the others
 * 2. Calls ptbl_merge to attempt to merge the entries
 * 3. Verifies that the entry remains unchanged (merge fails due to
 *    mismatched flags)
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)
#define TEST_MISMATCHED_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_USER)

void test_mismatched_idx(physaddr_t *ptbl_entry, struct page_table *ptbl, size_t idx)
{
	physaddr_t original_entry = *ptbl_entry;
	struct page_table original_ptbl;

	// Make sure all the entries are in a known state
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		ptbl->entries[i] = PAGE_ADDR(ptbl->entries[i]) | TEST_ENTRY_FLAGS;

	// Set one entry to be mismatched
	ptbl->entries[idx] = PAGE_ADDR(ptbl->entries[idx]) | TEST_MISMATCHED_FLAGS;

	// Save the ptbl state
	original_ptbl = *ptbl;

	assert(ptbl_merge(ptbl_entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	// Test that things remain unchanged
	assert(*ptbl_entry == original_entry);
	assert(memcmp(ptbl, &original_ptbl, sizeof(struct page_table)) == 0);
}

static int run_test()
{
	struct page_info *ptbl_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate and create our page table
	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref = 1;

	entry = page2pa(ptbl_page) | TEST_ENTRY_FLAGS;
	ptbl = page2kva(ptbl_page);

	// Allocate all the pages we will need
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		struct page_info *data_page = page_alloc(ALLOC_ZERO);
		assert(data_page != NULL);
		data_page->pp_ref += 1;
		ptbl->entries[i] = page2pa(data_page) | TEST_ENTRY_FLAGS;
	}

	// Test a few different interesting indices
	test_mismatched_idx(&entry, ptbl, 0);
	test_mismatched_idx(&entry, ptbl, PAGE_TABLE_ENTRIES - 1);
	test_mismatched_idx(&entry, ptbl, PAGE_TABLE_ENTRIES / 2);

	// Free the state
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
