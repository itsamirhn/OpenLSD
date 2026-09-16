#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "page_table_setup.h"

#define TEST_VA ((uintptr_t)0x200000)
#define TEST_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC)

static int run_test()
{
	struct paging_info info = {0};
	struct page_info *page;
	physaddr_t *entry;
	physaddr_t expected;

	assert(setup_page_tables(&info.tables[PML4], TEST_VA, PTE, &info) == 0);

	page = page_alloc(ALLOC_ZERO);
	page->pp_ref++;
	entry = &info.tables[PTE]->entries[PAGE_TABLE_INDEX(TEST_VA)];
	*entry = page2pa(page) | TEST_FLAGS;
	expected = PAGE_INVERT(page2pa(page));

	page_remove(info.tables[PML4], (void *)TEST_VA);

	assert(!(*entry & PAGE_PRESENT));
	assert(*entry == expected);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,
};
