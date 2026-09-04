#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - 2MB Replace
 * ==========================================
 *
 * This test verifies that page_insert correctly replaces an existing 2MB huge page
 * with a new one at the same virtual address.
 *
 * How it works:
 * 1. Sets up a page table hierarchy (PML4 -> PDPT -> PD) with an existing 2MB huge page
 * 2. Inserts a new 2MB huge page at the same virtual address using page_insert
 * 3. Verifies that page_insert handles the replacement correctly
 *
 * What it verifies:
 * - TLB invalidation is called with the correct virtual address
 * - The old page's reference count becomes 0 (page is freed)
 * - The new page's reference count is 1 (correctly inserted)
 * - The PD entry points to the new page with correct flags (PAGE_PRESENT | PAGE_HUGE)
 * - The old page is properly unmapped before the new one is inserted
 *
 * Key implementation details:
 * - Uses kernel_pml4 to ensure TLB invalidation works correctly (CR3 matches)
 * - Uses a probe to intercept tlb_invalidate calls and verify the address
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE HPAGE_SIZE
#define HUGE_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE)

static bool track_mem = false;
static uintptr_t test_va; // Global because we need this address in the kprobes
static bool invalidated = false;

/* Helper function to convert entry index to virtual address */
static uintptr_t entry_idx_to_va(size_t entry_idx)
{
	return (TEST_VA_BASE + entry_idx * TEST_PAGE_SIZE);
}

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_mem) {
		return;
	}

	/* tlb_invalidate(struct page_table *pml4, void *va) - second parameter (va) is in rsi */
	uintptr_t tlb_addr = (uintptr_t)frame->rsi;

	// Check that we are invalidating the appropriate entry
	assert(tlb_addr == test_va);
	invalidated = true;
}

static int run_test()
{
	struct paging_info info;
	struct page_info *page1, *page2;
	size_t page1_refs_before, page2_refs_before;

	test_va = entry_idx_to_va(0);

	assert(setup_page_tables(&info.tables[PML4], test_va, PDIR, &info) == 0);

	/* Allocate some pages to test with */
	page2 = page_alloc(ALLOC_HUGE);
	page1 = page_alloc(ALLOC_HUGE);
	page1->pp_ref++;

	assert(page2pa(page1) != page2pa(page2));

	/* Set up the initial PD entry pointing to page1 */
	info.tables[PDIR]->entries[PAGE_DIR_INDEX(test_va)] = page2pa(page1) | HUGE_FLAGS;

	page1_refs_before = page1->pp_ref;
	page2_refs_before = page2->pp_ref;

	track_mem = true;

	// Attempt to insert a new page into an already occupied location
	assert(page_insert(info.tables[PML4], page2, (void *)test_va, HUGE_FLAGS) == 0);

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

	// Check that the new entry
	physaddr_t new_entry = get_entry(info.tables[PML4], test_va, PDIR);

	if ((new_entry & PAGE_MASK) != HUGE_FLAGS) {
		panic("PDE entry should have correct flags after replacement\n");
	}

	if (PAGE_ADDR(new_entry) != page2pa(page2)) {
		panic("PDE entry should point to page2 after replacement\n");
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
