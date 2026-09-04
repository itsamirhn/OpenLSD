#include <assert.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/symbols.h>

/* Test Overview:
 * This test verifies PDIR-level behavior with a mix of huge pages and page tables.
 * The test structure:
 *
 * 1. Setup: Creates a page directory with:
 *    - Huge page entries (PAGE_HUGE flag) that map 2MB directly
 *    - Page table entries that point to page tables containing 4KB pages
 *
 * 2. Walk Range: The walk covers a range that includes both huge pages and page tables,
 *    partially covering the second page table to test boundary conditions.
 *
 * 3. Callbacks: Verifies:
 *    - pde_callback: Called for every PDIR entry in the walk range
 *    - pte_callback: Called for entries in page tables (not for huge pages)
 *
 * 4. Verification: Expected counts are computed from the walk range and the number
 *    of pages covered in each page table.
 */

#define FIRST_ENTRY_IDX 100
#define NUM_ENTRIES 5

/* Dummy physical addresses for huge page entries in tests.
 * These are arbitrary values used for huge page entries (entries with PAGE_HUGE flag).
 * We use dummy addresses for huge pages to avoid allocating 2MB pages which would
 * consume too much memory. The walker doesn't dereference huge page addresses,
 * it only reads the entry flags. */
#define TEST_BASE_PA 0x4000000

static struct {
	struct page_walker *expected_walker;
	struct page_table *pdir;
	size_t pde_callback_count;
	size_t pte_callback_count;
	size_t pde_unmap_count;
	size_t pt_hole_callback_count;
	uintptr_t last_base;
} callback_state;

static void reset_callback_state(struct page_table *pdir, uintptr_t base, struct page_walker *walker)
{
	memset(&callback_state, 0, sizeof(callback_state));
	callback_state.expected_walker = walker;
	callback_state.pdir = pdir;
	callback_state.last_base = base;
}

static int (*pdir_walk_range)(struct page_table *pdir, uintptr_t base,
							  uintptr_t end, struct page_walker *walker);

static uintptr_t entry_idx_to_va(int idx)
{
	return (uintptr_t)(idx * PAGE_TABLE_SPAN);
}

static int pde_callback(physaddr_t *entry, uintptr_t base, uintptr_t end,
						struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == HPAGE_SIZE - 1);
	assert((base & (HPAGE_SIZE - 1)) == 0);

	assert(callback_state.last_base <= base);
	callback_state.last_base = base;

	int pdpt_idx = PDPT_INDEX(base);
	int pdir_idx = PAGE_DIR_INDEX(base);
	assert(pdir_idx >= FIRST_ENTRY_IDX && pdir_idx <= FIRST_ENTRY_IDX + NUM_ENTRIES);

	if (*entry & PAGE_HUGE && *entry & PAGE_PRESENT) {
		uintptr_t expected_addr = TEST_BASE_PA + ((pdir_idx - FIRST_ENTRY_IDX) * HPAGE_SIZE);
		assert(PAGE_ADDR(*entry) == expected_addr);
	}

	assert(base >= pdpt_idx * PDPT_SPAN);
	assert(base <= (pdpt_idx * PDPT_SPAN) + PDPT_SPAN - 1);

	callback_state.pde_callback_count++;
	return 0;
}

static int pte_callback(physaddr_t *entry, uintptr_t base, uintptr_t end,
						struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == PAGE_SIZE - 1);
	assert((base & (PAGE_SIZE - 1)) == 0);

	assert(callback_state.last_base <= base);
	callback_state.last_base = base;

	int ptbl_idx = PAGE_TABLE_INDEX(base);
	int pdir_idx = PAGE_DIR_INDEX(base);
	assert(pdir_idx == FIRST_ENTRY_IDX + 1 || pdir_idx == FIRST_ENTRY_IDX + 4);

	if (*entry & PAGE_PRESENT) {
		uintptr_t expected_addr = TEST_BASE_PA + ((pdir_idx - FIRST_ENTRY_IDX) * HPAGE_SIZE) + ((ptbl_idx + 1) * PAGE_SIZE);
		assert(PAGE_ADDR(*entry) == expected_addr);
	}

	assert(base >= entry_idx_to_va(pdir_idx));
	assert(base <= entry_idx_to_va(pdir_idx) + PAGE_TABLE_SPAN - 1);

	callback_state.pte_callback_count++;
	return 0;
}

static int pde_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end,
					 struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == HPAGE_SIZE - 1);
	assert((base & (HPAGE_SIZE - 1)) == 0);

	assert(callback_state.last_base >= base);
	callback_state.pde_unmap_count++;
	return 0;
}

static int pt_hole_callback(uintptr_t base, uintptr_t end,
							struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);

	assert(callback_state.last_base <= base);
	callback_state.pt_hole_callback_count++;
	return 0;
}

static int run_test()
{
	struct page_table *pdir, *ptbl;
	struct page_info *pdir_page;
	struct page_info *pages[NUM_ENTRIES] = {0};
	uintptr_t base, end;
	uint64_t page_flags[NUM_ENTRIES] = {
		PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE,
		PAGE_PRESENT | PAGE_WRITE,
		PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE,
		PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE,
		PAGE_PRESENT | PAGE_WRITE,
	};

	pdir_page = page_alloc(ALLOC_ZERO);
	assert(pdir_page != NULL);
	pdir_page->pp_ref++;
	pdir = (struct page_table *)page2kva(pdir_page);

	// Create a page table structure with dummy entries
	for (size_t i = 0; i < NUM_ENTRIES; i++) {
		if (!(page_flags[i] & PAGE_HUGE)) {
			pages[i] = page_alloc(ALLOC_ZERO);
			assert(pdir_page != NULL);
			pages[i]->pp_ref++;
			base = page2pa(pages[i]);
			ptbl = page2kva(pages[i]);

			// Only fill page tables half way to make things more interesting
			for (size_t j = 0; j < PAGE_TABLE_ENTRIES / 2; j++)
				ptbl->entries[j] = TEST_BASE_PA + (i * HPAGE_SIZE) + ((j + 1) * PAGE_SIZE) | page_flags[i];
		} else {
			base = TEST_BASE_PA + (i * HPAGE_SIZE);
		}

		pdir->entries[FIRST_ENTRY_IDX + i] = base | page_flags[i];
	}

	base = entry_idx_to_va(FIRST_ENTRY_IDX);
	end = entry_idx_to_va(FIRST_ENTRY_IDX + NUM_ENTRIES) + PAGE_TABLE_SPAN - 1;

	struct page_walker walker = {
		.pde_callback = pde_callback,
		.pte_callback = pte_callback,
		.pde_unmap = pde_unmap,
		.pt_hole_callback = pt_hole_callback,
	};

	reset_callback_state(pdir, base, &walker);

	assert(pdir_walk_range(pdir, base, end, &walker) == 0);

	assert(callback_state.pde_callback_count == NUM_ENTRIES + 1);
	assert(callback_state.pte_callback_count == PAGE_TABLE_ENTRIES * 2);
	assert(callback_state.pde_unmap_count == NUM_ENTRIES);
	assert(callback_state.pt_hole_callback_count == PAGE_TABLE_ENTRIES + 1);
	assert(callback_state.last_base == end - HPAGE_SIZE + 1);

	// Free used state
	for (size_t i = 0; i < NUM_ENTRIES; i++) {
		if (pages[i])
			page_decref(pages[i]);
	}
	page_decref(pdir_page);

	return __checksum__;
}

extern int pml4_setup(struct boot_info *boot_info);
struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = pml4_setup,
	.should_continue = false,
	.symbol_count = 1,
	.symbols = (struct symbol_def[]) {
		{
			.name = "pdir_walk_range",
			.symbol = (void **)&pdir_walk_range,
			.type = ELF_SYM_TYPE_FUNC,
		},
	},
	.checksum = __checksum__,
};
