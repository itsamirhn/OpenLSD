#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *pml4_page, *pdpt_page, *pd_page;
	struct page_info *page_first, *page_middle, *page_last;
	struct page_table *pml4, *pdpt, *pd;
	void *test_va = (void *)0x400000;
	void *unmap_va = (void *)((uintptr_t)test_va + HPAGE_SIZE);
	uint64_t test_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	pml4_page->pp_ref++;
	pml4 = page2kva(pml4_page);
	
	pdpt_page = page_alloc(ALLOC_ZERO);
	pdpt_page->pp_ref++;
	pdpt = page2kva(pdpt_page);
	
	pd_page = page_alloc(ALLOC_ZERO);
	pd_page->pp_ref++;
	pd = page2kva(pd_page);
	
	page_first = page_alloc(ALLOC_HUGE);
	page_first->pp_ref++;
	page_middle = page_alloc(ALLOC_HUGE);
	page_middle->pp_ref++;
	page_last = page_alloc(ALLOC_HUGE);
	page_last->pp_ref++;
	
	size_t pml4_idx = PML4_INDEX((uintptr_t)test_va);
	size_t pdpt_idx = PDPT_INDEX((uintptr_t)test_va);
	
	pml4->entries[pml4_idx] = page2pa(pdpt_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pdpt->entries[pdpt_idx] = page2pa(pd_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	
	size_t pd_idx_first = PAGE_DIR_INDEX((uintptr_t)test_va);
	size_t pd_idx_middle = PAGE_DIR_INDEX((uintptr_t)unmap_va);
	size_t pd_idx_last = PAGE_DIR_INDEX((uintptr_t)test_va + 2 * HPAGE_SIZE);
	
	pd->entries[pd_idx_first] = page2pa(page_first) | test_flags;
	pd->entries[pd_idx_middle] = page2pa(page_middle) | test_flags;
	pd->entries[pd_idx_last] = page2pa(page_last) | test_flags;
	
	unmap_page_range(pml4, unmap_va, HPAGE_SIZE);
	
	if (!(pd->entries[pd_idx_first] & PAGE_PRESENT)) {
		panic("PDE entry %lu not present (first page should remain mapped)\n", pd_idx_first);
	}
	
	if (pd->entries[pd_idx_middle] & PAGE_PRESENT) {
		panic("PDE entry %lu still present after unmap\n", pd_idx_middle);
	}
	
	if (!(pd->entries[pd_idx_last] & PAGE_PRESENT)) {
		panic("PDE entry %lu not present (last page should remain mapped)\n", pd_idx_last);
	}
	
	uint64_t first_flags = pd->entries[pd_idx_first] & PAGE_MASK;
	uint64_t last_flags = pd->entries[pd_idx_last] & PAGE_MASK;
	if (first_flags != last_flags) {
		panic("PDE entry flags for first and last pages should be the same\n");
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
