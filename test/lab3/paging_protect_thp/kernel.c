#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int run_test() {
	struct page_info *page;
	physaddr_t *entry;
	size_t nfree;
	size_t i;
	void *ro_va = (void *)PAGE_SIZE;

	/* Save initial number of free pages */
	nfree = count_total_free_pages();

	/* Clear mappings in the first 2M range */
	unmap_page_range(kernel_pml4, 0, HPAGE_SIZE);

	if (page_lookup(kernel_pml4, 0, NULL)) {
		panic("expected no mapping at VA 0 after initial unmap");
	}

	/* Allocate a 2M huge page */
	page = page_alloc(ALLOC_HUGE);
	if (!page) {
		panic("failed to allocate 2M huge page");
	}

	/* Map it at VA 0 as read-write */
	if (page_insert(kernel_pml4, page, 0, PAGE_PRESENT | PAGE_WRITE) != 0) {
		panic("page_insert failed for 2M huge page at VA 0");
	}

	/* Check that VA 0 is mapped as a huge RW page */
	page = page_lookup(kernel_pml4, 0, &entry);
	if (!page) {
		panic("no page mapped at VA 0 after inserting 2M page");
	}
	if (!entry) {
		panic("PTE entry is NULL for VA 0 after inserting 2M page");
	}
	if (!(*entry & PAGE_PRESENT)) {
		panic("entry for VA 0 is not PRESENT after inserting 2M page (entry=%lx)",
		      (unsigned long)*entry);
	}
	if (!(*entry & PAGE_HUGE)) {
		panic("entry for VA 0 is not HUGE after inserting 2M page (entry=%lx)",
		      (unsigned long)*entry);
	}
	if (!(*entry & PAGE_WRITE)) {
		panic("entry for VA 0 is not WRITE after inserting 2M page (entry=%lx)",
		      (unsigned long)*entry);
	}
	if (PAGE_ADDR(*entry) != page2pa(page)) {
		panic("entry PA for VA 0 (%lx) does not match page2pa(page) (%lx)",
		      (unsigned long)PAGE_ADDR(*entry),
		      (unsigned long)page2pa(page));
	}
	if (page->pp_order != BUDDY_2M_PAGE) {
		panic("page at VA 0 has wrong order: %d (expected BUDDY_2M_PAGE=%d)",
		      page->pp_order, BUDDY_2M_PAGE);
	}
	if (page->pp_free) {
		panic("page at VA 0 is marked free but should be in use");
	}

	/* Change protection for one 4K page inside the 2M range to read-only */
	protect_region(kernel_pml4, ro_va, PAGE_SIZE, PAGE_PRESENT);

	/* After this, the huge page should be split into 4K pages.
	   Check all 4K pages in the 2M range. */
	for (i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
		void *va = (void *)(i * PAGE_SIZE);

		page = page_lookup(kernel_pml4, va, &entry);
		if (!page) {
			panic("no page mapped at VA %p after protect_region split (i=%zu)", va, i);
		}
		if (!entry) {
			panic("PTE entry is NULL for VA %p after protect_region split (i=%zu)", va, i);
		}
		if (!(*entry & PAGE_PRESENT)) {
			panic("entry for VA %p is not PRESENT after protect_region (entry=%lx, i=%zu)",
			      va, (unsigned long)*entry, i);
		}
		if (*entry & PAGE_HUGE) {
			panic("entry for VA %p is still HUGE after expected split (i=%zu)", va, i);
		}
		if (page->pp_order != 0) {
			panic("page at VA %p has order %d (expected 0) after split (i=%zu)",
			      va, page->pp_order, i);
		}
		if (page->pp_free) {
			panic("page at VA %p is marked free but should be in use (i=%zu)", va, i);
		}

		if (va == ro_va) {
			/* This one must be read-only */
			if (*entry & PAGE_WRITE) {
				panic("read-only VA %p still has WRITE bit set (entry=%lx)",
				      va, (unsigned long)*entry);
			}
		} else {
			/* All other 4K pages should still be writable */
			if (!(*entry & PAGE_WRITE)) {
				panic("VA %p lost WRITE permission unexpectedly (entry=%lx, i=%zu)",
				      va, (unsigned long)*entry, i);
			}
		}
	}

	/* Cleanup: unmap the whole 2M range */
	unmap_page_range(kernel_pml4, 0, HPAGE_SIZE);

	if (page_lookup(kernel_pml4, 0, NULL)) {
		panic("mapping at VA 0 still exists after final unmap");
	}
	if (kernel_pml4->entries[0] != 0) {
		panic("kernel_pml4->entries[0] not cleared after final unmap, value=%lx",
		      (unsigned long)kernel_pml4->entries[0]);
	}

	/* Check for memory leaks */
	if (nfree != count_total_free_pages()) {
		panic("free page count mismatch: before=%zu after=%zu",
		      nfree, count_total_free_pages());
	}

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
