/*
 * Test: ptbl_split with Static Huge Page
 *
 * This test verifies that ptbl_split correctly splits a static huge page
 * (a page that is not managed by the buddy allocator) into 512 individual
 * 4K pages. The split operation should create a page table where each entry
 * points to a contiguous region of the original huge page, preserving the
 * physical address layout. The static page should not be freed or have its
 * reference count modified.
 *
 * The test:
 * 1. Allocates a huge page and marks it as static (pp_ref = 0, pp_free = 0)
 * 2. Calls ptbl_split to split the static huge page
 * 3. Verifies that the entry now points to a page table with 512 entries
 * 4. Verifies that each entry points to the correct contiguous region of
 *    the original huge page
 * 5. Verifies that the static page remains unchanged (not freed)
 */

#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define TEST_VA_BASE 0
#define TEST_VA_END (PAGE_TABLE_SPAN - 1)
#define TEST_ENTRY_FLAGS (PAGE_PRESENT | PAGE_HUGE | PAGE_WRITE | PAGE_USER)

static bool track_pages = false;
static size_t pages_allocated = 0;

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

	// Decref should not be called during this test
	assert(false);
}

static void page_alloc_handler(struct probe_frame *frame)
{
	// Check that the test has started
	if (!track_pages)
		return;

	// Check that we are allocating a huge page
	assert((int)frame->rdi == (ALLOC_ZERO));

	// Track how many pages we are allocating
	pages_allocated++;
}

static int run_test()
{
	struct page_info *static_page, *ptbl_page;
	struct page_info original_page;
	struct page_table *ptbl;
	physaddr_t entry;
	size_t nfree = count_total_free_pages();

	// Allocate huge page as expected
	static_page = page_alloc(ALLOC_HUGE | ALLOC_ZERO);
	assert(static_page != NULL);
	static_page->pp_ref += 1;
	original_page = *static_page;

	// Create artificial huge static page
	static_page->pp_ref = 0;
	static_page->pp_free = 0;

	entry = page2pa(static_page) | TEST_ENTRY_FLAGS;

	// Initialize the huge page with unique patteerns
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		fill_page(page2kva(static_page) + (i * PAGE_SIZE), i & 0xFFFF, PAGE_SIZE);

	track_pages = true;

	// Attempt to split the static huge page
	assert(ptbl_split(&entry, TEST_VA_BASE, TEST_VA_END, NULL) == 0);

	track_pages = false;

	// Check that the new ptbl entry is sane
	assert(pa2page(PAGE_ADDR(entry)) != NULL);
	assert(pa2page(PAGE_ADDR(entry)) != static_page);
	assert((entry & PAGE_MASK) == (TEST_ENTRY_FLAGS & ~PAGE_HUGE));

	ptbl_page = pa2page(PAGE_ADDR(entry));
	assert(ptbl_page->pp_ref > 0 && ptbl_page->pp_avail);
	assert(ptbl_page->pp_order == BUDDY_4K_PAGE);

	ptbl = page2kva(ptbl_page);

	// Check that the entries are correctly formed
	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		assert((ptbl->entries[i] & PAGE_MASK) == (TEST_ENTRY_FLAGS & ~PAGE_HUGE));
		assert(PAGE_ADDR(ptbl->entries[i]) == page2pa(static_page) + (i * PAGE_SIZE));

		// Check that this page has the appropriate pattern
		assert(check_page(page2kva(pa2page(PAGE_ADDR(ptbl->entries[i]))), i & 0xFFFF, PAGE_SIZE));
	}

	// Check that our static page is still static
	assert(!static_page->pp_ref && !static_page->pp_free);
	assert(pages_allocated == 1);

	// Free the allocated pages
	page_decref(ptbl_page);

	// Restore the static page
	*static_page = original_page;
	page_decref(static_page);

	// Check for memory leaks
	assert(nfree == count_total_free_pages());

	return __checksum__;
}

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = page_init_ext,
    .should_continue = false,
    .checksum = __checksum__,

    .probe_count = 2,
    .probes = {
        {
            .target = page_decref,
            .callback = page_decref_handler,
        },
        {
            .target = page_alloc,
            .callback = page_alloc_handler,
        },
    },
};
