#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - 4KB Replace
 * =========================================
 * 
 * This test verifies that page_insert correctly replaces an existing 4KB page
 * with a new one at the same virtual address.
 * 
 * How it works:
 * 1. Sets up a complete page table hierarchy (PML4 -> PDPT -> PD -> PT) with an existing 4KB page
 * 2. Inserts a new 4KB page at the same virtual address using page_insert
 * 3. Verifies that page_insert handles the replacement correctly
 * 
 * What it verifies:
 * - TLB invalidation is called with the correct virtual address
 * - The old page's reference count becomes 0 (page is freed)
 * - The new page's reference count is 1 (correctly inserted)
 * - The PT entry points to the new page with correct flags
 * - The old page is properly unmapped before the new one is inserted
 * 
 * Key implementation details:
 * - Uses kernel_pml4 to ensure TLB invalidation works correctly (CR3 matches)
 * - Uses a probe to intercept tlb_invalidate calls and verify the address
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

static bool track_mem = false;
static bool invalidated = false;
static uintptr_t test_va;

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_mem) {
		return;
	}

	uintptr_t tlb_addr = (uintptr_t)frame->rsi;

	assert(tlb_addr == test_va);
	invalidated = true;
}

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *page1, *page2;
	size_t page1_refs_before, page2_refs_before;

	test_va = entry_idx_to_va(0);

	// Setup the appropriate paging structures in the kernel, so tlb_invalidate is easier to test
	assert(setup_page_tables(&info.tables[PML4], test_va, PTE, &info) == 0);

	/* Allocate the old 4KB page that will be replaced */
	page1 = page_alloc(ALLOC_ZERO);
	page1->pp_ref++;

	/* Allocate the new 4KB page that will replace the old one */
	page2 = page_alloc(ALLOC_ZERO);

	/* Set up the initial PT entry pointing to the old page */
	info.tables[PTE]->entries[PAGE_TABLE_INDEX(test_va)] = page2pa(page1) | TEST_FLAGS;

	page1_refs_before = page1->pp_ref;
	page2_refs_before = page2->pp_ref;

	track_mem = true;

	// Attempt to replace page1 with page2
	assert(page_insert(info.tables[PML4], page2, (void *)test_va, TEST_FLAGS) == 0);

	track_mem = false;

	if (!invalidated) {
		panic("tlb_invalidate should be called when replacing a page\n");
	}

	if (page1->pp_ref != (page1_refs_before - 1)) {
		panic("old page reference count should be 1 less after replacement\n");
	}

	if (page2->pp_ref != (page2_refs_before + 1)) {
		panic("new page reference count should be 1 more after insertion\n");
	}

	// Check to make sure the new entry is sane
	physaddr_t entry = get_entry(info.tables[PML4], test_va, PTE);

	if ((entry & PAGE_MASK) != TEST_FLAGS) {
		panic("PT entry should have correct flags after replacement\n");
	}

	if (PAGE_ADDR(entry) != page2pa(page2)) {
		panic("PT entry should point to new_page after replacement\n");
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = tlb_invalidate,
			.callback = tlb_invalidate_handler,
		},
	},
};
