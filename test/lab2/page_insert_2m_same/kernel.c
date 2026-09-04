#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

/*
 * Test Overview
 * =============
 * This test verifies that re-inserting the same 2MB huge page at the same
 * virtual address does not free the page.
 *
 * Test Structure:
 * - Allocates a 2MB huge page
 * - Inserts it into kernel_pml4 at address 0
 * - Re-inserts the same page at the same address
 * - Verifies page_free is not called on the page
 *
 * Walk Range Configuration:
 * - Single 2MB page at address 0
 *
 * Verification Approach:
 * - Uses page_free probe to verify the page is not freed
 * - Checks page reference count remains 1
 * - Confirms page_insert succeeds without freeing the page
 */

/* Test configuration constants */
#define TEST_VA ((void *)0)

static bool track_mem = false;
static bool page_freed = false;
static struct page_info *page;

static void page_free_handler(struct probe_frame *frame)
{
	if (!track_mem)
		return;

	struct page_info *page_arg = (struct page_info *)frame->rdi;

	// Not sure why you would free here otherwise, but just dont free the page we allocated
	if (page_arg == page)
		page_freed = true;
}

static int run_test()
{
	size_t refs_before;

	// Allocate a huge page to test with
	page = page_alloc(ALLOC_HUGE | ALLOC_ZERO);
	assert(page != NULL);

	refs_before = page->pp_ref;

	assert(page_insert(kernel_pml4, page, TEST_VA, PAGE_PRESENT) == 0);
	assert(page->pp_ref == (refs_before + 1));

	refs_before = page->pp_ref;
	track_mem = true;

	assert(page_insert(kernel_pml4, page, TEST_VA, PAGE_PRESENT) == 0);

	track_mem = false;

	assert(page->pp_ref == refs_before);
	if (page_freed) {
		panic("page_free was called on the same page when re-inserting at same VA");
	}

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = page_free,
			.callback = page_free_handler,
		},
	},
};
