#include <assert.h>
#include <paging.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *pml4_page, *pdpt_page, *pd_page, *pt_page_first, *pt_page_last;
	struct page_info *page;
	struct page_table *pml4, *pdpt, *pd, *pt_first, *pt_last;
	void *test_va = (void *)0x500000;
	uint64_t test_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC;
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
	
	pt_page_first = page_alloc(ALLOC_ZERO);
	pt_page_first->pp_ref++;
	pt_first = page2kva(pt_page_first);
	
	pt_page_last = page_alloc(ALLOC_ZERO);
	pt_page_last->pp_ref++;
	pt_last = page2kva(pt_page_last);
	
	size_t pml4_idx = PML4_INDEX((uintptr_t)test_va);
	size_t pdpt_idx = PDPT_INDEX((uintptr_t)test_va);
	size_t pd_idx_first = PAGE_DIR_INDEX((uintptr_t)test_va);
	
	pml4->entries[pml4_idx] = page2pa(pdpt_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pdpt->entries[pdpt_idx] = page2pa(pd_page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	pd->entries[pd_idx_first] = page2pa(pt_page_first) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	
	uintptr_t va = (uintptr_t)test_va;
	
	for (size_t i = 0; i < 256; i++) {
		page = page_alloc(ALLOC_ZERO);
		page->pp_ref++;
		size_t pt_idx = PAGE_TABLE_INDEX(va);
		pt_first->entries[pt_idx] = page2pa(page) | test_flags;
		va += PAGE_SIZE;
	}
	
	page = page_alloc(ALLOC_HUGE);
	page->pp_ref++;
	size_t pd_idx_huge1 = PAGE_DIR_INDEX(va);
	pd->entries[pd_idx_huge1] = page2pa(page) | huge_flags;
	va += HPAGE_SIZE;
	
	page = page_alloc(ALLOC_HUGE);
	page->pp_ref++;
	size_t pd_idx_huge2 = PAGE_DIR_INDEX(va);
	pd->entries[pd_idx_huge2] = page2pa(page) | huge_flags;
	va += HPAGE_SIZE;
	
	size_t pd_idx_last = PAGE_DIR_INDEX(va);
	pd->entries[pd_idx_last] = page2pa(pt_page_last) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	
	for (size_t i = 0; i < 256; i++) {
		page = page_alloc(ALLOC_ZERO);
		page->pp_ref++;
		size_t pt_idx = PAGE_TABLE_INDEX(va);
		pt_last->entries[pt_idx] = page2pa(page) | test_flags;
		va += PAGE_SIZE;
	}
	
	uintptr_t unmap_start = (uintptr_t)test_va + 128 * PAGE_SIZE;
	unmap_page_range(pml4, (void *)unmap_start, 128 * PAGE_SIZE);
	
	uintptr_t va_check = (uintptr_t)test_va;
	for (size_t i = 0; i < 128; i++) {
		size_t pt_idx = PAGE_TABLE_INDEX(va_check);
		if (!(pt_first->entries[pt_idx] & PAGE_PRESENT)) {
			panic("PT entry %lu not present (should remain mapped) for VA %p\n", pt_idx, (void *)va_check);
		}
		va_check += PAGE_SIZE;
	}
	
	for (size_t i = 0; i < 128; i++) {
		size_t pt_idx = PAGE_TABLE_INDEX(va_check);
		if (pt_first->entries[pt_idx] & PAGE_PRESENT) {
			panic("PT entry %lu still present after unmap for VA %p\n", pt_idx, (void *)va_check);
		}
		va_check += PAGE_SIZE;
	}
	
	va_check = (uintptr_t)test_va + 256 * PAGE_SIZE;
	if (!(pd->entries[PAGE_DIR_INDEX(va_check)] & PAGE_PRESENT)) {
		panic("PDE entry for first huge page not present\n");
	}
	
	va_check += HPAGE_SIZE;
	if (!(pd->entries[PAGE_DIR_INDEX(va_check)] & PAGE_PRESENT)) {
		panic("PDE entry for second huge page not present\n");
	}
	
	va_check += HPAGE_SIZE;
	for (size_t i = 0; i < 256; i++) {
		size_t pt_idx = PAGE_TABLE_INDEX(va_check);
		if (!(pt_last->entries[pt_idx] & PAGE_PRESENT)) {
			panic("PT entry %lu not present (should remain mapped) for VA %p\n", pt_idx, (void *)va_check);
		}
		va_check += PAGE_SIZE;
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
