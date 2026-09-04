#include <assert.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	struct page_info *pml4_page;
	struct page_info *lookup_page;
	struct page_table *pml4;
	void *test_va = (void *)0x200000;
	physaddr_t *entry_store = NULL;
	
	pml4_page = page_alloc(ALLOC_ZERO);
	pml4_page->pp_ref++;
	pml4 = page2kva(pml4_page);
	
	lookup_page = page_lookup(pml4, test_va, &entry_store);
	
	if (lookup_page) {
		panic("page_lookup should return NULL for unmapped address\n");
	}
	
	if (entry_store) {
		panic("entry_store should be NULL for unmapped address\n");
	}
	
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
