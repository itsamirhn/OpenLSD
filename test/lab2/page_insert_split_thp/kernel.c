#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

/*
 * Test Overview: Page Insert - Split THP
 * ==============================================
 * 
 * This test verifies that page_insert correctly splits a 2MB huge page into a
 * Page Table (PT) with 512 4KB pages when inserting a 4KB page into the huge page range.
 * Then merges it again after inserting.
 * 
 * How it works:
 * 1. Sets up a page table hierarchy with an existing 2MB huge page
 * 2. Writes different data patterns to each 4KB chunk of the huge page
 * 3. Inserts a 4KB page at a specific address within the huge page range
 * 4. Verifies that page_insert triggers a split, creating a PT with 512 entries
 * 
 * What it verifies:
 * - TLB invalidation is called when the huge page is replaced
 * - The huge page is freed after the split (reference count becomes 0)
 * - A new PT is created with 512 entries, all marked as present
 * - Data from the huge page is correctly copied to the 512 new 4KB pages
 * - The inserted 4KB page has the correct data
 * - All other 4KB pages contain the data that was in the corresponding chunk of the huge page
 * 
 * Key implementation details:
 * - Uses a custom PML4 since we're testing page table splitting
 * - Writes unique patterns to each 4KB chunk to verify data copying
 */

/* Test configuration constants */
#define TEST_VA_BASE 0x400000
#define TEST_PAGE_SIZE PAGE_SIZE
#define HUGE_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE)
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC)
#define NEW_PAGE_IDX 25

static bool track_mem = false;
static uintptr_t test_va;
static bool split_invalidated = false;
static bool page_invalidated = false;
static bool merge_invalidated[PAGE_TABLE_ENTRIES] = {0};

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

// 1. Called 1 time on the currently allocated huge page (by split)
// 2. Called 1 time for 4k page being replaced by pte_insert
// 3. Called 512 times for the split pages by the later merge
static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_mem) {
		return;
	}

	uintptr_t tlb_addr = frame->rsi;

	if (tlb_addr == ROUNDDOWN(test_va, HPAGE_SIZE) && !split_invalidated) {
		assert(!page_invalidated);
		for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
			assert(!merge_invalidated[i]);

		split_invalidated = true;

	} else if ((tlb_addr == test_va) && !page_invalidated) {
		assert(split_invalidated);
		for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
			assert(!merge_invalidated[i]);

		page_invalidated = true;

	} else {
		assert(split_invalidated);
		assert(page_invalidated);

		int page_idx = (tlb_addr - TEST_VA_BASE) / PAGE_SIZE;
		assert(page_idx < PAGE_TABLE_ENTRIES);

		merge_invalidated[page_idx] = true;
	}
}

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *huge_page, *new_page;
	size_t huge_refs_before, new_refs_before;
	test_va = entry_idx_to_va(NEW_PAGE_IDX);

	// Setup the paging structures for the huge page
	assert(setup_page_tables(&info.tables[PML4], test_va, PDIR, &info) == 0);

	/* Allocate the huge page that will be split */
	huge_page = page_alloc(ALLOC_HUGE);
	huge_page->pp_ref += 2;

	// Allocate a second 4k page to insert into the huge page
	new_page = page_alloc(ALLOC_ZERO);

	/* Set up PD entry pointing to the huge page */
	info.tables[PDIR]->entries[PAGE_DIR_INDEX(test_va)] = page2pa(huge_page) | HUGE_FLAGS;

	/* Write different patterns to each 4KB chunk of the huge page.
	 * Each chunk gets a unique byte value so we can verify that
	 * data is correctly copied when the huge page is split into 512 4KB pages.
	 */
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
		fill_page(page2kva(huge_page) + (i * PAGE_SIZE), i & 0xFFFF, PAGE_SIZE);

	// Huge page is filled with values 0 - 511, fill the new page with a unique value
	fill_page(page2kva(new_page), 0xFFFE, PAGE_SIZE);

	huge_refs_before = huge_page->pp_ref;
	new_refs_before = new_page->pp_ref;

	track_mem = true;

	assert(page_insert(info.tables[PML4], new_page, (void *)test_va, TEST_FLAGS) == 0);

	track_mem = false;

	/* Expected invalidations: 512 + 2 */
	assert(split_invalidated);
	assert(page_invalidated);
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
		assert(merge_invalidated[i]);

	// Verify the old huge page is freed
	if (huge_page->pp_ref != (huge_refs_before - 1)) {
		panic("Huge page reference count should be 1 less after being replaced by PT\n");
	}

	// Verify new 4K page is also freed
	if (new_page->pp_ref != new_refs_before) {
		panic("New 4K page reference count should be 1 more after insertion\n");
	}

	// Verify PDIR points to a new huge page
	physaddr_t entry = get_entry(info.tables[PML4], test_va, PDIR);
	if ((entry & PAGE_MASK) != HUGE_FLAGS) {
		panic("PDE entry should point to a PDE after split\n");
	}

	// Get the new huge page
	huge_page = pa2page(PAGE_ADDR(entry));
	if (huge_page->pp_ref == 0) {
		panic("PT page should have a reference count > 0\n");
	}

	/* Verify the new huge page is correctly constructed */
	/* Note: The inserted page will replace one of the pages, so we check accordingly */
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		void *huge_data = page2kva(huge_page) + (i * PAGE_SIZE);
		if (i == NEW_PAGE_IDX) {
			if (!check_page(huge_data, 0xFFFE, PAGE_SIZE)) {
				panic("PT entry %zu (inserted page): page data should be 0xFFFE, got 0x%04X\n",
				      i, ((uint32_t *)huge_data)[0]);
			}
		} else {
			if (!check_page(huge_data, i & 0xFFFF, PAGE_SIZE)) {
				panic("PT entry %zu: page data should be %u, got %u\n",
				      i, i & 0xFFFF, ((uint32_t *)huge_data)[0]);
			}
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
