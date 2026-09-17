/*
 * Test: ptbl_merge Order
 *
 * This test verifies that ptbl_merge correctly handles the order of operations
 * when merging a page table. Specifically, it verifies that page_decref is called
 * with the correct huge page that contains all the merged data. This ensures
 * that the merge operation completes successfully before the old page table
 * is freed.
 *
 * The test:
 * 1. Allocates a page table with 512 present entries, each with test data
 * 2. Sets up a probe to intercept page_decref calls and tlb_invalidate calls
 * 3. Calls ptbl_merge to merge the entries
 * 4. Verifies that page_decref was called with the correct huge page containing
 *    all the merged data
 * 5. Verifies that tlb_invalidate is called the correct amount of times with the correct
 *    addresses
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

static bool pages_freed[PAGE_TABLE_ENTRIES] = {0};
static bool pages_invalidated[PAGE_TABLE_ENTRIES] = {0};
static bool huge_allocated = false;
static struct page_info *ptbl_page = 0; // Needs to be global so we can see it in the kprobe
static bool track_pages = false;

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

static void page_decref_handler(struct probe_frame *frame)
{
	if (!track_pages)
		return;

	struct page_info *pp = (struct page_info *)frame->rdi;

	// Some sanity checks so we dont fault here
	assert(pp);
	assert(pp->pp_avail);
	assert(!pp->pp_free);
	assert(pp->pp_ref > 0);

	// Check that we are not freeing the original ptbl page
	if (pp == ptbl_page)
		return;

	// Derive ptbl index from pattern
	int page_idx = ((int *)page2kva(pp))[0];
	assert(page_idx < PAGE_TABLE_ENTRIES);

	// Check decref ordering
	assert(huge_allocated);

	pages_freed[page_idx] = true;
}

static void page_alloc_handler(struct probe_frame *frame)
{
	// Check that the test has started
	if (!track_pages)
		return;

	// Check that none of our pages have been freed yet
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
		assert(!pages_freed[i] && !pages_invalidated[i]);

	// Check that we are allocating a huge page
	assert((int)frame->rdi & ALLOC_HUGE);
	huge_allocated = true;
}

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_pages)
		return;

	int page_idx = (frame->rsi - TEST_VA_BASE) / PAGE_SIZE;
	assert(page_idx < PAGE_TABLE_ENTRIES);

	// Make sure we are calling tlb invalidate at the right moment
	assert(!pages_freed[page_idx]);
	assert(huge_allocated);

	pages_invalidated[page_idx] = true;
}

static int run_test()
{
	struct page_info *data_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate and intialize our full page table
	ptbl_page = page_alloc(ALLOC_ZERO);
	assert(ptbl_page != NULL);
	ptbl_page->pp_ref += 1;

	entry = page2pa(ptbl_page) | TEST_ENTRY_FLAGS;
	ptbl = page2kva(ptbl_page);

	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		data_page = page_alloc(ALLOC_ZERO);
		assert(data_page != NULL);
		data_page->pp_ref += 1;
		ptbl->entries[i] = page2pa(data_page) | TEST_ENTRY_FLAGS;

		// Fill each page with a unique pattern
		fill_page(page2kva(data_page), i & 0xFFFF, PAGE_SIZE);

		// Some book keeping
		pages_freed[i] = false;
	}

	// Let our kprobes start to track how merge behaves
	track_pages = true;

	// Attempt to merge the pages together
	assert(ptbl_merge(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	track_pages = false;

	// Check that the new entry is not the same as the old one, and has correct flags
	assert(entry != (page2pa(ptbl_page) | TEST_ENTRY_FLAGS));
	assert((entry & PAGE_MASK) == (TEST_ENTRY_FLAGS | PAGE_HUGE));

	// Check that the huge page is correct
	data_page = pa2page(PAGE_ADDR(entry));
	assert(data_page != NULL);
	assert(data_page->pp_ref == 1);
	assert(data_page->pp_order == BUDDY_2M_PAGE);
	assert(huge_allocated);

	// Check that data was copied over in the correct order, and all previous pages were freed
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		assert(pages_freed[i] && pages_invalidated[i]);
		assert(check_page(page2kva(data_page) + (i * PAGE_SIZE), i & 0xFFFF, PAGE_SIZE));
	}

	// Free our state
	page_decref(data_page);

	// Check for memory leaks
	assert(nfree == count_total_free_pages());

	return __checksum__;
}

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = page_init_ext,
    .should_continue = false,
    .checksum = __checksum__,

    .probe_count = 3,
    .probes = {
        {
            .target = page_decref,
            .callback = page_decref_handler,
        },
        {
            .target = page_alloc,
            .callback = page_alloc_handler,
        },
        {
            .target = tlb_invalidate,
            .callback = tlb_invalidate_handler,
        },
    },
};
