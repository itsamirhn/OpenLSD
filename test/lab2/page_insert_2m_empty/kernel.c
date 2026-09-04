#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview
 * =============
 * This test verifies inserting a 2MB huge page into an empty page table structure.
 *
 * Test Structure:
 * - Creates a fresh PML4 page table
 * - Inserts a single 2MB huge page at a test virtual address
 * - Verifies the page table structure is correctly created (PML4 -> PDPT -> PD)
 * - Verifies the PD entry has the correct flags and points to the inserted page
 *
 * Walk Range Configuration:
 * - Single 2MB page insertion at a fixed test address
 *
 * Verification Approach:
 * - Checks that all intermediate page table levels are created
 * - Verifies the final PD entry has PAGE_HUGE flag set
 * - Confirms the physical address in the entry matches the inserted page
 * - Validates page reference count is correct
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE HPAGE_SIZE
#define HUGE_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE)

static int run_test()
{
	struct page_table *pml4 = NULL;
	struct page_info *page = page_alloc(ALLOC_HUGE);
	uintptr_t test_va = TEST_VA_BASE;

	// Setup the pml4
	assert(setup_pml4(&pml4, NULL) == 0);

	size_t refs_before = page->pp_ref;

	// Try to insert a new page here
	assert(page_insert(pml4, page, (void *)test_va, HUGE_FLAGS) == 0);

	if (page->pp_ref != (refs_before + 1)) {
		panic("page reference count should be 1 more after insertion\n");
	}

	// Check that all the appropriate table entries are present
	physaddr_t pd_entry = get_entry(pml4, test_va, PDIR);
	assert(pd_entry != 0);

	if (PAGE_ADDR(pd_entry) != page2pa(page)) {
		panic("PD entry should point to the inserted page\n");
	}

	if ((pd_entry & PAGE_MASK) != HUGE_FLAGS) {
		panic("PD entry should have correct flags\n");
	}

	return __checksum__;
}

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = page_init_ext,
    .should_continue = false,
    .checksum = __checksum__,
};
