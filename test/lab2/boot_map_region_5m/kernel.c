#include <assert.h>
#include <paging.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern int pml4_setup(struct boot_info *boot_info);

static int run_test() {
	struct page_info *pml4_page;
	struct page_table *pml4;
	/* Use huge page aligned addresses to avoid misalignment issues */
	physaddr_t test_pa = ROUNDDOWN(0x300000, HPAGE_SIZE);
	uintptr_t test_va_uint = ROUNDDOWN(0x500000, HPAGE_SIZE);
	void *test_va = (void *)test_va_uint;
	size_t test_size = 5 * 1024 * 1024;
	uint64_t test_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	if (!pml4_page) {
		panic("cannot allocate PML4 page!");
	}
	pml4 = page2kva(pml4_page);
	
	boot_map_region(pml4, test_va, test_size, test_pa, test_flags);
	
	uintptr_t va_start = ROUNDDOWN((uintptr_t)test_va, PAGE_SIZE);
	uintptr_t va_end = ROUNDUP((uintptr_t)test_va + test_size, PAGE_SIZE);
	physaddr_t pa_current = ROUNDDOWN(test_pa, PAGE_SIZE);
	
	uintptr_t va = va_start;
	while (va < va_end) {
		size_t pml4_idx = PML4_INDEX(va);
		if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
			panic("PML4 entry %lu not present for VA %p\n", pml4_idx, (void *)va);
		}
		
		struct page_table *pdpt = (struct page_table *)KADDR(PAGE_ADDR(pml4->entries[pml4_idx]));
		size_t pdpt_idx = PDPT_INDEX(va);
		if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
			panic("PDPT entry %lu not present for VA %p\n", pdpt_idx, (void *)va);
		}
		
		struct page_table *pd = (struct page_table *)KADDR(PAGE_ADDR(pdpt->entries[pdpt_idx]));
		size_t pd_idx = PAGE_DIR_INDEX(va);
		
		if (pd->entries[pd_idx] & PAGE_PRESENT && pd->entries[pd_idx] & PAGE_HUGE) {
			physaddr_t pde_entry = pd->entries[pd_idx];
			uint64_t pde_flags = pde_entry & PAGE_MASK;
			physaddr_t pde_pa = PAGE_ADDR(pde_entry);
			
			uint64_t expected_flags = test_flags | PAGE_HUGE;
			if (pde_flags != expected_flags) {
				panic("PDE flags mismatch for VA %p: expected 0x%llx, got 0x%llx\n",
				      (void *)va, (unsigned long long)expected_flags, (unsigned long long)pde_flags);
			}
			
			if (pde_pa != pa_current) {
				panic("PDE physical address mismatch for VA %p: expected %p, got %p\n",
				      (void *)va, (void *)pa_current, (void *)pde_pa);
			}
			
			pa_current += HPAGE_SIZE;
			va = ROUNDUP(va + 1, HPAGE_SIZE);
			continue;
		}
		
		if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
			panic("PD entry %lu not present for VA %p\n", pd_idx, (void *)va);
		}
		
		struct page_table *pt = (struct page_table *)KADDR(PAGE_ADDR(pd->entries[pd_idx]));
		size_t pt_idx = PAGE_TABLE_INDEX(va);
		if (!(pt->entries[pt_idx] & PAGE_PRESENT)) {
			panic("PT entry %lu not present for VA %p\n", pt_idx, (void *)va);
		}
		
		physaddr_t pte_entry = pt->entries[pt_idx];
		uint64_t pte_flags = pte_entry & PAGE_MASK;
		physaddr_t pte_pa = PAGE_ADDR(pte_entry);
		
		if (pte_flags != test_flags) {
			panic("PTE flags mismatch for VA %p: expected 0x%llx, got 0x%llx\n",
			      (void *)va, (unsigned long long)test_flags, (unsigned long long)pte_flags);
		}
		
		if (pte_pa != pa_current) {
			panic("PTE physical address mismatch for VA %p: expected %p, got %p\n",
			      (void *)va, (void *)pa_current, (void *)pte_pa);
		}
		
		pa_current += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = pml4_setup,
	.should_continue = false,
	.checksum = __checksum__,
};
