#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview
 * =============
 * This test verifies inserting a 4KB page into an empty page table structure.
 *
 * Test Structure:
 * - Creates a fresh PML4 page table
 * - Inserts a single 4KB page at a test virtual address
 * - Verifies the page table structure is correctly created (PML4 -> PDPT -> PD -> PT)
 * - Verifies the PT entry has the correct flags and points to the inserted page
 *
 * Walk Range Configuration:
 * - Single 4KB page insertion at a fixed test address
 *
 * Verification Approach:
 * - Checks that all intermediate page table levels are created
 * - Verifies the final PT entry is present
 * - Confirms the physical address in the entry matches the inserted page
 * - Validates page reference count is correct
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x200000
#define TEST_PAGE_SIZE PAGE_SIZE
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_NO_EXEC)

/* Helper function to convert entry index to virtual address */
static uintptr_t entry_idx_to_va(size_t entry_idx)
{
	return (TEST_VA_BASE + entry_idx * TEST_PAGE_SIZE);
}

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *page;
	uintptr_t test_va = entry_idx_to_va(0);

	// Setup only the pml4
	assert(setup_pml4(&info.tables[PML4], &info) == 0);

	// Allocate a page to test with
	page = page_alloc(ALLOC_ZERO);

	size_t refs_before = page->pp_ref;

	// Attempt to insert page at test_va
	assert(page_insert(info.tables[PML4], page, (void *)test_va, TEST_FLAGS) == 0);

	if (page->pp_ref != 1) {
		panic("page reference count should be 1 after insertion\n");
	}

	// Check that the appropriate entries were created at each level
	if (!(get_entry(info.tables[PML4], test_va, PML4) & PAGE_PRESENT)) {
		panic("PML4 entry should be present after insertion\n");
	}

	if (!(get_entry(info.tables[PML4], test_va, PDPT) & PAGE_PRESENT)) {
		panic("PDPT entry should be present after insertion\n");
	}

	if (!(get_entry(info.tables[PML4], test_va, PDIR) & PAGE_PRESENT)) {
		panic("PD entry should be present after insertion\n");
	}

	if (!(get_entry(info.tables[PML4], test_va, PTE) & PAGE_PRESENT)) {
		panic("PT entry should be present after insertion\n");
	}

	physaddr_t entry = get_entry(info.tables[PML4], test_va, PTE);

	// Check that the created entry has the right flags
	if ((entry & PAGE_MASK) != TEST_FLAGS) {
		panic("PT entry should have correct flags\n");
	}

	// Check that the new entry points to the correct page
	if (PAGE_ADDR(entry) != page2pa(page)) {
		panic("PT entry should point to the inserted page\n");
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,
};
