#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *pml4_page, *pdpt_page, *pd_page;
	struct page_info *page;
	struct page_table *pml4, *pdpt, *pd;
	void *test_va = (void *)0x400000;
	uint64_t huge_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC | PAGE_HUGE;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	pml4_page->pp_ref++;
	pml4 = page2kva(pml4_page);
	
	pdpt_page = page_alloc(ALLOC_ZERO);
	pdpt_page->pp_ref++;
	pdpt = page2kva(pdpt_page);
	
	pd_page = page_alloc(ALLOC_ZERO);
	pd_page->pp_ref++;
	pd = page2kva(pd_page);
	
	page = page_alloc(ALLOC_HUGE);
	page->pp_ref++;
	
	size_t pml4_idx = PML4_INDEX((uintptr_t)test_va);
	size_t pdpt_idx = PDPT_INDEX((uintptr_t)test_va);
	size_t pd_idx = PAGE_DIR_INDEX((uintptr_t)test_va);
	
	pml4->entries[pml4_idx] = page2pa(pdpt_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pdpt->entries[pdpt_idx] = page2pa(pd_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pd->entries[pd_idx] = page2pa(page) | huge_flags;
	
	unmap_page_range(pml4, test_va, HPAGE_SIZE);
	
	if (pml4->entries[pml4_idx] & PAGE_PRESENT) {
		panic("PML4 entry should be cleared after all lower levels are freed\n");
	}
	
	if (pdpt_page->pp_ref != 0) {
		panic("PDPT page should have reference count 0 after being freed\n");
	}
	
	if (pd_page->pp_ref != 0) {
		panic("PD page should have reference count 0 after being freed\n");
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
