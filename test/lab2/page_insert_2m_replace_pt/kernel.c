#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>
#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - 2MB Replace PT
 * ============================================
 * 
 * This test verifies that page_insert correctly replaces an existing Page Table (PT)
 * containing multiple 4KB pages with a single 2MB huge page.
 * 
 * How it works:
 * 1. Sets up a page table hierarchy with a PT containing 3 existing 4KB pages
 * 2. Inserts a 2MB huge page at the base address, which should replace the entire PT
 * 3. Verifies that page_insert handles the replacement correctly
 * 
 * What it verifies:
 * - TLB invalidation is called for each replaced 4KB page (at least 3 times)
 * - The PT page is freed (reference count becomes 0)
 * - All 4KB pages are freed (reference counts become 0)
 * - The PD entry now points to the huge page with PAGE_HUGE flag
 * - The huge page's reference count is 1
 * 
 * Key implementation details:
 * - Uses a custom PML4 since we're testing page table replacement
 * - Uses a probe to count TLB invalidations
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE PAGE_SIZE
#define NUM_PAGES 3
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC)

/* Helper function to convert entry index to virtual address */
static uintptr_t entry_idx_to_va(size_t entry_idx)
{
	return (TEST_VA_BASE + entry_idx * TEST_PAGE_SIZE);
}

static int invalidate_count = 0;
static bool track_mem = false;

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_mem)
		return;

	invalidate_count++;
}

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *pages[NUM_PAGES];
	size_t refs_before[NUM_PAGES];
	struct page_info *huge_page;
	uintptr_t test_va[NUM_PAGES] = {
		entry_idx_to_va(0),
		entry_idx_to_va(1),
		entry_idx_to_va(2),
	};

	/* Allocate and set up a custom PML4 for this test */
	assert(setup_page_tables(&info.tables[PML4], test_va[0], PTE, &info) == 0);

	/* Allocate the huge page that will replace the PT */
	huge_page = page_alloc(ALLOC_HUGE);

	/* Allocate and insert three 4KB pages that will be replaced by the huge page */
	for (size_t i = 0; i < 3; i++) {
		pages[i] = page_alloc(ALLOC_ZERO);
		pages[i]->pp_ref++;
		refs_before[i] = pages[i]->pp_ref;
		info.tables[PTE]->entries[PAGE_TABLE_INDEX(test_va[i])] = page2pa(pages[i]) | TEST_FLAGS;
	}

	size_t huge_refs_before = huge_page->pp_ref;
	size_t pt_refs_before = info.pages[PTE]->pp_ref;
	track_mem = true;

	// Attempt to insert the huge page, replacing the three existing entries
	assert(page_insert(info.tables[PML4], huge_page, (void *)test_va[0], TEST_FLAGS | PAGE_HUGE) == 0);

	track_mem = false;

	/* Expected invalidations: one per existing 4KB page */
	if (invalidate_count < NUM_PAGES) {
		panic("tlb_invalidate should be called at least %d times (once per 4KB page), was called %d times\n", NUM_PAGES, invalidate_count);
	}

	if (info.pages[PTE]->pp_ref != (pt_refs_before - 1)) {
		panic("PT page reference count should be 1 less after being replaced by huge page\n");
	}

	/* Verify all old pages are freed */
	for (size_t i = 0; i < NUM_PAGES; i++) {
		if (pages[i]->pp_ref != (refs_before[i] - 1)) {
			panic("Page %zu reference count should be 1 less after being replaced by huge page\n", i + 1);
		}
	}

	if (huge_page->pp_ref != (huge_refs_before + 1)) {
		panic("Huge page reference count should be 1 more after insertion\n");
	}

	physaddr_t new_entry = get_entry(info.tables[PML4], test_va[0], PDIR);
	if ((new_entry & PAGE_MASK) != (TEST_FLAGS | PAGE_HUGE)) {
		panic("PDE entry should have correct flags after huge page insertion\n");
	}

	if (PAGE_ADDR(new_entry) != page2pa(huge_page)) {
		panic("PDE entry should point to huge_page after insertion\n");
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
