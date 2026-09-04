#include <assert.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - Merge THP
 * =======================================
 * 
 * This test verifies that page_insert correctly merges 512 4KB pages into a single
 * 2MB huge page when filling the last empty entry in a Page Table (Transparent Huge Page merge).
 * 
 * How it works:
 * 1. Sets up a page table hierarchy with a PT containing 511 4KB pages (one hole at index 25)
 * 2. Writes unique data patterns to each page to verify data copying
 * 3. Inserts a 4KB page to fill the hole, which should trigger a THP merge
 * 4. Verifies that all 512 pages are merged into a single 2MB huge page
 * 
 * What it verifies:
 * - TLB invalidation is called for all 512 pages (at least 512 times)
 * - The PT page is freed after the merge (reference count becomes 0)
 * - All 512 4KB pages are freed (reference counts become 0)
 * - The PD entry now points to a huge page with PAGE_HUGE flag
 * - The huge page contains the correct data from all original 4KB pages
 * - Data integrity is maintained: each 4KB chunk of the huge page has the correct pattern
 * 
 * Key implementation details:
 * - Uses a custom PML4 since we're testing THP merge behavior
 * - Leaves one entry empty to trigger the merge when filled
 * - Writes unique patterns to each page to verify data copying during merge
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE PAGE_SIZE
#define HUGE_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE)
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC)
#define MISSING_PAGE_IDX 25

/* Helper function to convert entry index to virtual address */
static uintptr_t entry_idx_to_va(size_t entry_idx)
{
	return (TEST_VA_BASE + entry_idx * TEST_PAGE_SIZE);
}

static inline void fill_page(void *page, int c, size_t n)
{
	assert(n % 4 == 0);
	asm volatile("cld; rep stosl\n" ::"D"(page), "a"(c), "c"(n / 4)
	             : "cc", "memory");
}

static inline bool check_page(void *page, int c, size_t n)
{
	size_t remaining = n / 4;
	assert(n % 4 == 0);

	asm volatile("cld; repe scasl\n"
	             : "+D"(page), "+c"(remaining)
	             : "a"(c)
	             : "cc");

	return remaining == 0;
}

static bool track_mem = false;
static uintptr_t pages_invalidated[PAGE_TABLE_ENTRIES] = {0};

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_mem) {
		return;
	}

	int page_idx = (frame->rsi - TEST_VA_BASE) / PAGE_SIZE;
	assert(page_idx < PAGE_TABLE_ENTRIES);

	pages_invalidated[page_idx] = true;
}

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *pages[PAGE_TABLE_ENTRIES];
	size_t page_refs_before[PAGE_TABLE_ENTRIES];
	uintptr_t test_va = entry_idx_to_va(MISSING_PAGE_IDX);

	// Setup the needed page tables
	assert(setup_page_tables(&info.tables[PML4], test_va, PTE, &info) == 0);

	/* Allocate 512 4KB pages (one for each entry except the hole).
	 * Each page gets a unique data pattern so we can verify
	 * that data is correctly copied when merging into a huge page.
	 * One page is not inserted yet
	 */
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		pages[i] = page_alloc(ALLOC_ZERO);
		fill_page(page2kva(pages[i]), i & 0xFFFF, PAGE_SIZE);

		// Insert this page if it is not missing
		if (i != MISSING_PAGE_IDX) {
			pages[i]->pp_ref++;
			info.tables[PTE]->entries[i] = page2pa(pages[i]) | TEST_FLAGS;
		}

		page_refs_before[i] = pages[i]->pp_ref;
	}

	size_t pt_refs_before = info.pages[PTE]->pp_ref;
	track_mem = true;

	// Attempt to insert the
	assert(page_insert(info.tables[PML4], pages[MISSING_PAGE_IDX], (void *)test_va, TEST_FLAGS) == 0);

	track_mem = false;

	/* Expected invalidations: one per page in the PT */
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
		assert(pages_invalidated[i]);

	if (info.pages[PTE]->pp_ref != (pt_refs_before - 1)) {
		panic("PT page reference count should be 1 less after merge\n");
	}

	/* Verify all pages pages are freed */
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		if (i == MISSING_PAGE_IDX) {
			assert(pages[i]->pp_ref == page_refs_before[i]);
		} else {
			assert(pages[i]->pp_ref == (page_refs_before[i] - 1));
		}
	}

	// Verify the new flags are correct
	physaddr_t entry = get_entry(info.tables[PML4], test_va, PDIR);
	if ((entry & PAGE_MASK) != HUGE_FLAGS) {
		panic("PDE entry should have correct flags after merge\n");
	}

	// Check that the huge page has appropriate refs
	struct page_info *huge_page = pa2page(PAGE_ADDR(entry));
	if (huge_page->pp_ref == 0) {
		panic("Huge page reference count should be non-zero after merge\n");
	}

	// Because the missing page with the missing pattern was inserted, everything should line up now
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		if (!check_page(page2kva(huge_page) + (i * PAGE_SIZE), i % 0xFFFF, PAGE_SIZE)) {
			panic("Huge page data at offset %u should be 0x%x, got 0x%x\n",
			      i * PAGE_SIZE, i & 0xFFFF, ((uint32_t *)page2kva(huge_page))[0]);
		}
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
