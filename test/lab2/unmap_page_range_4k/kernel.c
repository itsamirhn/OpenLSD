#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *pml4_page, *pdpt_page, *pd_page, *pt_page;
	struct page_info *page_first, *page_middle, *page_last;
	struct page_table *pml4, *pdpt, *pd, *pt;
	void *test_va = (void *)0x200000;
	void *unmap_va = (void *)((uintptr_t)test_va + PAGE_SIZE);
	uint64_t test_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	pml4_page->pp_ref++;
	pml4 = page2kva(pml4_page);
	
	pdpt_page = page_alloc(ALLOC_ZERO);
	pdpt_page->pp_ref++;
	pdpt = page2kva(pdpt_page);
	
	pd_page = page_alloc(ALLOC_ZERO);
	pd_page->pp_ref++;
	pd = page2kva(pd_page);
	
	pt_page = page_alloc(ALLOC_ZERO);
	pt_page->pp_ref++;
	pt = page2kva(pt_page);
	
	page_first = page_alloc(ALLOC_ZERO);
	page_first->pp_ref++;
	page_middle = page_alloc(ALLOC_ZERO);
	page_middle->pp_ref++;
	page_last = page_alloc(ALLOC_ZERO);
	page_last->pp_ref++;
	
	size_t pml4_idx = PML4_INDEX((uintptr_t)test_va);
	size_t pdpt_idx = PDPT_INDEX((uintptr_t)test_va);
	size_t pd_idx = PAGE_DIR_INDEX((uintptr_t)test_va);
	
	pml4->entries[pml4_idx] = page2pa(pdpt_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pdpt->entries[pdpt_idx] = page2pa(pd_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pd->entries[pd_idx] = page2pa(pt_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	
	size_t pt_idx_first = PAGE_TABLE_INDEX((uintptr_t)test_va);
	size_t pt_idx_middle = PAGE_TABLE_INDEX((uintptr_t)unmap_va);
	size_t pt_idx_last = PAGE_TABLE_INDEX((uintptr_t)test_va + 2 * PAGE_SIZE);
	
	pt->entries[pt_idx_first] = page2pa(page_first) | test_flags;
	pt->entries[pt_idx_middle] = page2pa(page_middle) | test_flags;
	pt->entries[pt_idx_last] = page2pa(page_last) | test_flags;
	
	unmap_page_range(pml4, unmap_va, PAGE_SIZE);
	
	if (!(pt->entries[pt_idx_first] & PAGE_PRESENT)) {
		panic("PT entry %lu not present (first page should remain mapped)\n", pt_idx_first);
	}
	
	if (pt->entries[pt_idx_middle] & PAGE_PRESENT) {
		panic("PT entry %lu still present after unmap\n", pt_idx_middle);
	}
	
	if (!(pt->entries[pt_idx_last] & PAGE_PRESENT)) {
		panic("PT entry %lu not present (last page should remain mapped)\n", pt_idx_last);
	}
	
	uint64_t first_flags = pt->entries[pt_idx_first] & PAGE_MASK;
	uint64_t last_flags = pt->entries[pt_idx_last] & PAGE_MASK;
	if (first_flags != last_flags) {
		panic("PT entry flags for first and last pages should be the same\n");
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
