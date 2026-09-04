#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - 4KB Existing
 * ==========================================
 * 
 * This test verifies that page_insert correctly handles inserting 4KB pages
 * when higher-level page tables (PDPT, PD, PT) already exist.
 * 
 * How it works:
 * 1. Sets up a complete page table hierarchy (PML4 -> PDPT -> PD -> PT) with one existing page entry
 * 2. Re-inserts the same page at test_va1 (should not free the page)
 * 3. Inserts a new page at test_va2 in the same PT
 * 4. Verifies both operations succeed and entries are correctly set up
 * 
 * What it verifies:
 * - Re-inserting the same page at the same address doesn't free it (ref count stays 1)
 * - Inserting a new page at a different address in the same PT works correctly
 * - Both PT entries have correct flags and point to the correct physical pages
 * - Both entries are marked as present
 * 
 * Key implementation details:
 * - Uses a custom PML4 (not kernel_pml4) since we're testing page table setup
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x200000
#define TEST_PAGE_SIZE PAGE_SIZE
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC)

/* Helper function to convert entry index to virtual address */
static uintptr_t entry_idx_to_va(size_t entry_idx)
{
	return (TEST_VA_BASE + entry_idx * TEST_PAGE_SIZE);
}

static int run_test()
{
	struct paging_info info;
	struct page_info *page1, *page2;
	size_t refs_before;
	physaddr_t entry;
	uintptr_t test_va1 = entry_idx_to_va(0);
	uintptr_t test_va2 = entry_idx_to_va(1);

	// Set up the paging hierarchy up to PTE
	assert(setup_page_tables(&info.tables[PML4], test_va1, PTE, &info) == 0);

	/* Allocate two 4KB pages: one existing, one new */
	page1 = page_alloc(ALLOC_ZERO);
	page1->pp_ref++;

	page2 = page_alloc(ALLOC_ZERO);

	/* Set up initial PT entry with the first page */
	info.tables[PTE]->entries[PAGE_TABLE_INDEX(test_va1)] = page2pa(page1) | TEST_FLAGS;

	refs_before = page1->pp_ref;

	// Attempt to re-insert the first page in the same location
	assert(page_insert(info.tables[PML4], page1, (void *)test_va1, TEST_FLAGS) == 0);

	if (page1->pp_ref != refs_before) {
		panic("page1 reference count should be the same after re-insertion\n");
	}

	refs_before = page2->pp_ref;

	// Attempt to insert the new page into a separate slot
	assert(page_insert(info.tables[PML4], page2, (void *)test_va2, TEST_FLAGS) == 0);

	if (page2->pp_ref != (refs_before + 1)) {
		panic("page2 reference count should be 1 more after insertion\n");
	}

	// Check that the entry for page2 is correct
	entry = get_entry(info.tables[PML4], test_va2, PTE);
	if ((entry & PAGE_MASK) != TEST_FLAGS) {
		panic("PT entry for test_va2 should be present\n");
	}

	if (PAGE_ADDR(entry) != page2pa(page2)) {
		panic("PT entry for test_va2 should point to page2\n");
	}

	// Check that the entry for page 1 is still correct
	entry = get_entry(info.tables[PML4], test_va1, PTE);
	if ((entry & PAGE_MASK) != TEST_FLAGS) {
		panic("PT entry for test_va1 should be present\n");
	}

	if (PAGE_ADDR(entry) != page2pa(page1)) {
		panic("PT entry for test_va1 should point to page1\n");
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,
};
