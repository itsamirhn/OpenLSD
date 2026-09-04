#include <assert.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/symbols.h>

/* Test Overview:
 * This test verifies PDIR-level callbacks. The test structure:
 *
 * 1. Setup: Creates a page directory (PDIR) with some entries present and some not.
 *    Present entries can be either huge pages (PAGE_HUGE flag) or point to page tables.
 *    The walk range extends beyond the test entry range to verify boundary handling.
 *
 * 2. Walk Range: The walk covers entries from WALK_START_IDX to WALK_END_IDX.
 *    Within this range, entries at FIRST_ENTRY_IDX to LAST_ENTRY_IDX have specific
 *    attributes (present/not present, huge page flags, etc.).
 *
 * 3. Callbacks: Each callback verifies:
 *    - pde_callback: Called for every PDIR entry in the walk range
 *    - pt_hole_callback: Called for entries without PAGE_PRESENT flag
 *    - pde_unmap: Called for all present entries (both huge and non-huge) after processing
 *
 * 4. Verification: Expected counts are computed by iterating through the walk range
 *    and checking entry attributes. For present non-huge entries, the walker descends
 *    into the page table, but this test focuses on PDIR-level behavior.
 */

/* Define which pages will be walked */
#define ENTRIES_BEFORE_RANGE 2
#define ENTRIES_AFTER_RANGE 2
#define FIRST_ENTRY_IDX 100
#define LAST_ENTRY_IDX 103
#define NUM_ENTRIES (LAST_ENTRY_IDX - FIRST_ENTRY_IDX + 1)

/* Compute walk range boundaries */
#define WALK_START_IDX (FIRST_ENTRY_IDX - ENTRIES_BEFORE_RANGE)
#define WALK_END_IDX (LAST_ENTRY_IDX + ENTRIES_AFTER_RANGE)
#define WALK_RANGE_SIZE (WALK_END_IDX - WALK_START_IDX + 1)

/* Dummy physical addresses for huge page entries in tests.
 * These are arbitrary values used for huge page entries (entries with PAGE_HUGE flag).
 * We use dummy addresses for huge pages to avoid allocating 2MB pages which would
 * consume too much memory. The walker doesn't dereference huge page addresses,
 * it only reads the entry flags. */
#define TEST_BASE_PA 0x1000000

static struct {
	struct page_table *pdir;
	struct page_walker *expected_walker;
	size_t callback_count;
	size_t hole_count;
	size_t unmap_count;
	size_t pte_callback_count;
	uintptr_t last_base;
} callback_state;

static int (*pdir_walk_range)(struct page_table *pdir, uintptr_t base,
	uintptr_t end, struct page_walker *walker);

static uintptr_t entry_idx_to_va(int idx) {
	return (uintptr_t)(idx * PAGE_TABLE_SPAN);
}

static size_t entries_with_flag(struct page_table *tbl, uint64_t flags)
{
	size_t count = 0;
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
		count += (tbl->entries[i] & flags) == flags ? 1 : 0;

	return count;
}

static void reset_callback_state(struct page_table *pdir, struct page_walker *walker,
                                 uintptr_t start_base)
{
	memset(&callback_state, 0, sizeof(callback_state));
	callback_state.pdir = pdir;
	callback_state.expected_walker = walker;
	callback_state.last_base = start_base;
}

static int pde_callback(physaddr_t *entry, uintptr_t base, uintptr_t end,
                        struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == PAGE_TABLE_SPAN - 1);
	assert((base & (PAGE_TABLE_SPAN - 1)) == 0);

	assert(base >= callback_state.last_base);
	callback_state.last_base = base;

	int entry_idx = PAGE_DIR_INDEX(base);
	assert(entry == &callback_state.pdir->entries[entry_idx]);

	callback_state.callback_count++;
	return 0;
}

static int pt_hole_callback(uintptr_t base, uintptr_t end,
                            struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(base >= callback_state.last_base);

	callback_state.hole_count++;

	return 0;
}

static int pde_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end,
                     struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == PAGE_TABLE_SPAN - 1);
	assert((base & (PAGE_TABLE_SPAN - 1)) == 0);
	assert(*entry & PAGE_PRESENT);

	assert(base <= callback_state.last_base);

	callback_state.unmap_count++;
	return 0;
}

static int pt_callback(physaddr_t *entry, uintptr_t base, uintptr_t end,
                       struct page_walker *walker)
{
	assert(walker == callback_state.expected_walker);
	assert(end - base == PAGE_SIZE - 1);
	assert((base & (PAGE_SIZE - 1)) == 0);

	assert(base >= callback_state.last_base);
	callback_state.last_base = base;

	callback_state.pte_callback_count++;
	return 0;
}

static int run_test()
{
	struct page_table *pdir;
	struct page_info *pdir_page, *pte_page;
	uintptr_t walk_base, walk_end;
	uint64_t entries[NUM_ENTRIES] = {
		PAGE_WRITE,
		PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE,
		PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC,
		PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE | PAGE_NO_EXEC
	};

	pdir_page = page_alloc(ALLOC_ZERO);
	assert(pdir_page != NULL);
	pdir_page->pp_ref++;

	pdir = (struct page_table *)page2kva(pdir_page);

	// Allocate a dummy page for the ptbl entry
	pte_page = page_alloc(ALLOC_ZERO);
	assert(pte_page != NULL);
	pte_page->pp_ref++;

	// Create some dummy page entries in this pdir
	for (int i = 0; i < NUM_ENTRIES; i++)
		pdir->entries[FIRST_ENTRY_IDX + i] = (TEST_BASE_PA + i * HPAGE_SIZE) | entries[i];

	pdir->entries[FIRST_ENTRY_IDX + 2] = page2pa(pte_page) | entries[2];

	/* Compute walk range from test configuration */
	walk_base = entry_idx_to_va(WALK_START_IDX);
	walk_end = entry_idx_to_va(WALK_END_IDX) + PAGE_TABLE_SPAN - 1;

	// Define the needed walker
	struct page_walker walker = {
		.pde_callback = pde_callback,
		.pte_callback = pt_callback,
		.pde_unmap = pde_unmap,
		.pt_hole_callback = pt_hole_callback,
	};

	reset_callback_state(pdir, &walker, walk_base);

	assert(pdir_walk_range(pdir, walk_base, walk_end, &walker) == 0);

	// Check that callbacks were invoked appropriately
	assert(callback_state.callback_count == WALK_RANGE_SIZE);
	assert(callback_state.hole_count == PAGE_TABLE_ENTRIES + WALK_RANGE_SIZE - entries_with_flag(pdir, PAGE_PRESENT));
	assert(callback_state.unmap_count == entries_with_flag(pdir, PAGE_PRESENT));
	assert(callback_state.last_base == walk_end - HPAGE_SIZE + 1);

	// Check that the subrange was walked appropriately
	size_t num_pte = entries_with_flag(pdir, PAGE_PRESENT) - entries_with_flag(pdir, PAGE_PRESENT | PAGE_HUGE);
	assert(callback_state.pte_callback_count == num_pte * PAGE_TABLE_ENTRIES);

	// Check that the pdir entries are unchanged
	for (int i = 0; i < NUM_ENTRIES; i++)
		assert((pdir->entries[FIRST_ENTRY_IDX + i] & PAGE_UMASK) == (entries[i] & PAGE_UMASK));

	// Clean up used state
	page_decref(pte_page);
	page_decref(pdir_page);

	return __checksum__;
}

extern int pml4_setup(struct boot_info *boot_info);
struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = pml4_setup,
	.should_continue = false,
	.symbol_count = 1,
	.symbols = (struct symbol_def[]){
		{
			.name = "pdir_walk_range",
			.symbol = (void **)&pdir_walk_range,
			.type = ELF_SYM_TYPE_FUNC,
		},
	},
	.checksum = __checksum__,
};
