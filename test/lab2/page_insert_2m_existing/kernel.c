#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - 2MB Existing
 * ==========================================
 *
 * This test verifies that page_insert correctly handles inserting 2MB huge pages
 * when higher-level page tables (PDPT, PD) already exist.
 *
 * How it works:
 * 1. Sets up a page table hierarchy (PML4 -> PDPT -> PD) with one existing huge page entry
 * 2. Re-inserts the same page at test_va1 (should not free the page)
 * 3. Inserts a new huge page at test_va2 in the same PD
 * 4. Verifies both operations succeed and entries are correctly set up
 *
 * What it verifies:
 * - Re-inserting the same page at the same address doesn't free it (ref count stays 1)
 * - Inserting a new page at a different address in the same PD works correctly
 * - Both PD entries have correct flags (PAGE_PRESENT | PAGE_HUGE)
 * - Both entries point to the correct physical pages
 *
 * Key implementation details:
 * - Uses a custom PML4 (not kernel_pml4) since we're testing page table setup
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE HPAGE_SIZE
#define HUGE_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE)

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
	uintptr_t test_va1 = entry_idx_to_va(0);
	uintptr_t test_va2 = entry_idx_to_va(1);

	// Setup pml4, pdpt, and pd for va1, va2 should use the same tables
	assert(setup_page_tables(&info.tables[PML4], test_va1, PDIR, &info) == 0);

	/* Allocate two huge pages */
	page2 = page_alloc(ALLOC_HUGE);
	page1 = page_alloc(ALLOC_HUGE);
	page1->pp_ref++;

	assert(page2pa(page2) != page2pa(page1));

	/* Set up initial PD entry with the first huge page */
	info.tables[PDIR]->entries[PAGE_DIR_INDEX((uintptr_t)test_va1)] = page2pa(page1) | HUGE_FLAGS;

	refs_before = page1->pp_ref;

	// Attempt to re-insert the same page in the same spot
	assert(page_insert(info.tables[PML4], page1, (void *)test_va1, HUGE_FLAGS) == 0);

	if (page1->pp_ref != refs_before) {
		panic("page1 reference count should be the same after attempted re-insertion\n");
	}

	refs_before = page2->pp_ref;

	// Attempt to insert a new page into a free slot
	assert(page_insert(info.tables[PML4], page2, (void *)test_va2, HUGE_FLAGS) == 0);

	if (page2->pp_ref != (refs_before + 1)) {
		panic("page2 reference count should be 1 more after insertion\n");
	}

	// Get the PDIR entries for page1 and page2
	physaddr_t page1_entry = get_entry(info.tables[PML4], test_va1, PDIR);
	physaddr_t page2_entry = get_entry(info.tables[PML4], test_va2, PDIR);

	// Check that we can retrieve the pages correctly
	assert(page1_entry != 0 && page2_entry != 0);

	// Check that the flags for va1 and va2 are correct
	if ((page1_entry & PAGE_MASK) != (PAGE_PRESENT | HUGE_FLAGS)) {
		panic("PDE entry for test_va1 should have correct flags\n");
	}

	if ((page2_entry & PAGE_MASK) != (PAGE_PRESENT | HUGE_FLAGS)) {
		panic("PDE entry for test_va2 should have correct flags\n");
	}

	// Check that the addresses are correct for va1 and va2
	if (PAGE_ADDR(page1_entry) != page2pa(page1)) {
		panic("PDE entry for test_va2 should point to page2\n");
	}

	if (PAGE_ADDR(page2_entry) != page2pa(page2)) {
		panic("PDE entry for test_va1 should point to page1\n");
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,
};
