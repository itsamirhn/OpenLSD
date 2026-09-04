#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern int pml4_setup(struct boot_info *boot_info);

static int run_test() {
	struct page_info *pml4_page;
	struct page_table *pml4;
	physaddr_t test_pa = 0x200000;
	void *test_va = (void *)0x400000;
	size_t test_size = HPAGE_SIZE;
	uint64_t test_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	if (!pml4_page) {
		panic("cannot allocate PML4 page!");
	}
	pml4 = page2kva(pml4_page);
	
	boot_map_region(pml4, test_va, test_size, test_pa, test_flags);
	
	size_t pml4_idx = PML4_INDEX((uintptr_t)test_va);
	if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
		panic("PML4 entry %lu not present\n", pml4_idx);
	}
	
	struct page_table *pdpt = (struct page_table *)KADDR(PAGE_ADDR(pml4->entries[pml4_idx]));
	size_t pdpt_idx = PDPT_INDEX((uintptr_t)test_va);
	if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
		panic("PDPT entry %lu not present\n", pdpt_idx);
	}
	
	struct page_table *pd = (struct page_table *)KADDR(PAGE_ADDR(pdpt->entries[pdpt_idx]));
	size_t pd_idx = PAGE_DIR_INDEX((uintptr_t)test_va);
	if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
		panic("PD entry %lu not present\n", pd_idx);
	}
	
	if (!(pd->entries[pd_idx] & PAGE_HUGE)) {
		panic("PD entry %lu should be a huge page for 2MB mapping\n", pd_idx);
	}
	
	physaddr_t pde_entry = pd->entries[pd_idx];
	uint64_t pde_flags = pde_entry & PAGE_MASK;
	physaddr_t pde_pa = PAGE_ADDR(pde_entry);
	
	uint64_t expected_flags = test_flags | PAGE_HUGE;
	if (pde_flags != expected_flags) {
		panic("PDE flags mismatch: expected 0x%llx, got 0x%llx\n",
		      (unsigned long long)expected_flags, (unsigned long long)pde_flags);
	}
	
	if (pde_pa != test_pa) {
		panic("PDE physical address mismatch: expected %p, got %p\n",
		      (void *)test_pa, (void *)pde_pa);
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = pml4_setup,
	.should_continue = false,
	.checksum = __checksum__,
};
