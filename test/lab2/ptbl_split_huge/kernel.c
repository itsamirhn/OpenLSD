/*
 * Test: ptbl_split with Huge Page
 *
 * This test verifies that ptbl_split correctly splits a 2M huge page into
 * 512 individual 4K pages. The split operation should copy all data from the
 * huge page into the individual pages, allocate a new page table, free the
 * old huge page, and update the entry to point to the page table without the
 * PAGE_HUGE flag. It also verifies that TLB invalidation is called with the
 * correct address.
 *
 * The test:
 * 1. Allocates a huge page with test data
 * 2. Sets up a probe to intercept tlb_invalidate calls
 * 3. Calls ptbl_split to split the huge page
 * 4. Verifies that the entry now points to a page table with 512 entries
 * 5. Verifies that all data from the huge page is preserved in the split pages
 * 6. Verifies that the old huge page is freed
 * 7. Verifies that TLB invalidation is called with the correct address
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_HUGE | PAGE_WRITE | PAGE_USER)

static size_t pages_allocated = 0;
static struct page_info *huge_page = 0; // Needs to be global so we can see it in the kprobe
static bool track_pages = false;
static bool huge_freed = false;
static bool tlb_invalidated = false;

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
	assert(pp->pp_order == BUDDY_2M_PAGE);

	// Check that we are only freeing after all pages have been allocated
	assert(pages_allocated == (PAGE_TABLE_ENTRIES + 1));
	assert(pp == huge_page);
	huge_freed = true;
}

static void page_alloc_handler(struct probe_frame *frame)
{
	// Check that the test has started
	if (!track_pages)
		return;

	// Check that we are not allocating a huge page
	assert(~((int)frame->rdi & PAGE_HUGE));
	assert(!huge_freed);

	// Track how many pages we are allocating
	pages_allocated++;
}

static void tlb_invalidate_handler(struct probe_frame *frame)
{
	if (!track_pages)
		return;

	// Make sure we are calling tlb invalidate at the right moment
	assert(pages_allocated == (PAGE_TABLE_ENTRIES + 1)); // +1 for the ptbl page
	assert(!huge_freed);

	// Check that we are invalidating the huge pages addr
	assert((void *)frame->rsi == TEST_VA_BASE);

	tlb_invalidated = true;
}

static int run_test()
{
	struct page_info *data_page, *ptbl_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	huge_page = page_alloc(ALLOC_HUGE | ALLOC_ZERO);
	assert(huge_page != NULL);
	huge_page->pp_ref += 1;

	// Create the ptbl entry
	entry = page2pa(huge_page) | TEST_ENTRY_FLAGS;

	// Initialize the huge page with unique patteerns
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		fill_page(page2kva(huge_page) + (i * PAGE_SIZE), i & 0xFFFF, PAGE_SIZE);

	track_pages = true;

	// Attempt to split the huge page
	assert(ptbl_split(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	track_pages = false;

	// Check that the new entry is sane
	assert((entry & PAGE_MASK) == (TEST_ENTRY_FLAGS & ~PAGE_HUGE));
	assert(PAGE_ADDR(entry) != page2pa(huge_page));
	assert(pa2page(PAGE_ADDR(entry)) != NULL);

	// Get the newly created page table
	ptbl_page = pa2page(PAGE_ADDR(entry));
	ptbl = page2kva(ptbl_page);

	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		// Check that flags are as expected
		assert((ptbl->entries[i] & PAGE_MASK) == (TEST_ENTRY_FLAGS & ~PAGE_HUGE));

		data_page = pa2page(PAGE_ADDR(ptbl->entries[i]));
		assert(data_page != NULL);
		assert(data_page->pp_ref && data_page->pp_avail && !data_page->pp_free);

		// Extra check to ensure that the new page is not part of the old huge page
		assert((page2pa(data_page) < page2pa(huge_page)) || page2pa(data_page) >= page2pa(huge_page) + HPAGE_SIZE);

		// Check that this page has the appropriate pattern
		assert(check_page(page2kva(data_page), i & 0xFFFF, PAGE_SIZE));
	}

	// Check that everything was called correctly
	assert(pages_allocated == (PAGE_TABLE_ENTRIES + 1));
	assert(tlb_invalidated);
	assert(huge_freed);

	// Free the currently allocated pages
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		page_decref(pa2page(PAGE_ADDR(ptbl->entries[i])));

	page_decref(ptbl_page);

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
