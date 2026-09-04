#include <assert.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/symbols.h>
#include <kernel/test/test.h>

/* Test Overview:
 * This test verifies error handling in PDIR-level callbacks. The test structure:
 *
 * 1. Setup: Creates a page directory with huge page entries.
 *
 * 2. Error Scenarios: Tests four error conditions:
 *    - pde_callback returns error: walker should stop immediately
 *    - pt_hole_callback returns error: walker should stop when encountering a hole
 *    - pde_unmap returns error: walker should stop after processing a present entry
 *    - pte_callback returns error: walker should stop after ptbl_walk_range returns an error
 *
 * 3. Verification: Each test verifies that:
 *    - The correct error code is returned
 *    - Only the expected callbacks were invoked before the error
 */

#define NUM_ENTRIES 10
#define FIRST_ENTRY_IDX 100
#define WALK_START_IDX FIRST_ENTRY_IDX
#define WALK_END_IDX (FIRST_ENTRY_IDX + NUM_ENTRIES - 1)

/* Dummy physical addresses for huge page entries in tests.
 * These are arbitrary values used for huge page entries (entries with PAGE_HUGE flag).
 * We use dummy addresses for huge pages to avoid allocating 2MB pages which would
 * consume too much memory. The walker doesn't dereference huge page addresses,
 * it only reads the entry flags. */
#define TEST_BASE_PA 0x1000000

// Some errors, so we know what was called
#define ERROR_PDE_CALLBACK -1
#define ERROR_PT_HOLE_CALLBACK -2
#define ERROR_PDE_UNMAP -3
#define ERROR_PTE_CALLBACK -4

static struct {
	size_t callback_count;
	int callback_error;
	size_t hole_count;
	int hole_error;
	size_t unmap_count;
	int unmap_error;
	size_t pte_callback_count;
	int pte_callback_error;
} callback_state;

static int (*pdir_walk_range)(struct page_table *pdir, uintptr_t base,
	uintptr_t end, struct page_walker *walker);

static uintptr_t entry_idx_to_va(int idx)
{
	return (uintptr_t)(idx * PAGE_TABLE_SPAN);
}

static void reset_callback_state(void)
{
	memset(&callback_state, 0, sizeof(callback_state));
	callback_state.callback_error = -1;
	callback_state.hole_error = -1;
	callback_state.unmap_error = -1;
	callback_state.pte_callback_error = -1;
}

static int pde_callback(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker)
{
	callback_state.callback_count++;
	if (PAGE_DIR_INDEX(base) == callback_state.callback_error)
		return ERROR_PDE_CALLBACK;

	return 0;
}

static int pt_hole_callback(uintptr_t base, uintptr_t end,
				struct page_walker *walker)
{
	callback_state.hole_count++;
	if (PAGE_DIR_INDEX(base) == callback_state.hole_error)
		return ERROR_PT_HOLE_CALLBACK;

	return 0;
}

static int pde_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end,
			struct page_walker *walker)
{
	callback_state.unmap_count++;
	if (PAGE_DIR_INDEX(base) == callback_state.unmap_error)
		return ERROR_PDE_UNMAP;

	return 0;
}

static int pte_callback(physaddr_t *entry, uintptr_t base, uintptr_t end,
	struct page_walker *walker)
{
	callback_state.pte_callback_count++;
	if (PAGE_TABLE_INDEX(base) == callback_state.pte_callback_error)
		return ERROR_PTE_CALLBACK;

	return 0;
}

static int run_test()
{
	struct page_table *pdir;
	struct page_info *pdir_page, *ptbl_page;
	uintptr_t walk_base, walk_end;
	uint64_t error_idx = (NUM_ENTRIES / 2);

	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref++;

	pdir_page = page_alloc(ALLOC_ZERO);
	assert(pdir_page != NULL);
	pdir_page->pp_ref++;
	pdir = (struct page_table *)page2kva(pdir_page);

	// Initialize the pdir with some dummy values
	for (int i = 0; i < NUM_ENTRIES; i++)
		pdir->entries[FIRST_ENTRY_IDX + i] = (TEST_BASE_PA + i * HPAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;

	walk_base = entry_idx_to_va(WALK_START_IDX);
	walk_end = entry_idx_to_va(WALK_END_IDX) + PAGE_TABLE_SPAN - 1;

	struct page_walker walker = {
		.pde_callback = pde_callback,
		.pt_hole_callback = pt_hole_callback,
		.pde_unmap = pde_unmap,
	};

	// Test with an error in the callback
	reset_callback_state();
	callback_state.callback_error = FIRST_ENTRY_IDX + error_idx;

	assert(pdir_walk_range(pdir, walk_base, walk_end, &walker) == ERROR_PDE_CALLBACK);
	assert(callback_state.callback_count == error_idx + 1);
	assert(callback_state.hole_count == 0);
	assert(callback_state.unmap_count == error_idx);

	// Test with the error in unmap
	reset_callback_state();
	callback_state.unmap_error = FIRST_ENTRY_IDX + error_idx;

	assert(pdir_walk_range(pdir, walk_base, walk_end, &walker) == ERROR_PDE_UNMAP);
	assert(callback_state.callback_count == error_idx + 1);
	assert(callback_state.hole_count == 0);
	assert(callback_state.unmap_count == error_idx + 1);

	// Test if the error occurs when walking a subrange in a ptbl
	reset_callback_state();
	walker.pte_callback = pte_callback;
	callback_state.pte_callback_error = FIRST_ENTRY_IDX + error_idx;
	pdir->entries[FIRST_ENTRY_IDX + error_idx] = page2pa(ptbl_page) | PAGE_PRESENT | PAGE_WRITE;

	assert(pdir_walk_range(pdir, walk_base, walk_end, &walker) == ERROR_PTE_CALLBACK);
	assert(callback_state.callback_count == error_idx + 1);
	assert(callback_state.hole_count == error_idx + FIRST_ENTRY_IDX);
	assert(callback_state.unmap_count == error_idx);
	assert(callback_state.pte_callback_count == error_idx + FIRST_ENTRY_IDX + 1);

	/* Create holes for hole callback error test */
	pdir->entries[FIRST_ENTRY_IDX] = 0;
	pdir->entries[FIRST_ENTRY_IDX + 1] = 0;
	pdir->entries[FIRST_ENTRY_IDX + error_idx] = 0;

	reset_callback_state();
	callback_state.hole_error = FIRST_ENTRY_IDX + error_idx;
	walker.pte_callback = NULL;

	assert(pdir_walk_range(pdir, walk_base, walk_end, &walker) == ERROR_PT_HOLE_CALLBACK);
	assert(callback_state.callback_count == error_idx + 1);
	assert(callback_state.hole_count == 3);
	assert(callback_state.unmap_count == error_idx - 2);

	// Free used state
	page_decref(ptbl_page);
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
