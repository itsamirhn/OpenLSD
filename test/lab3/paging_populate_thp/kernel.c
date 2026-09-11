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

    /* Save number of free pages so we can detect leaks later */
    nfree = count_total_free_pages();

    /* Remove any mapping in the first 2MB virtual range */
    unmap_page_range(kernel_pml4, 0, HPAGE_SIZE);

    /* Check we really removed all mappings */
    if (page_lookup(kernel_pml4, 0, NULL)) {
        panic("expected no mapping at VA 0 after initial unmap");
    }

    /* Loop through all 4KB pages except the last one */
    for (i = 0; i < PAGE_TABLE_ENTRIES - 1; ++i) {
        void *va = (void *)(i * PAGE_SIZE);

        /* Populate only this single 4KB page */
        populate_region(kernel_pml4, va, PAGE_SIZE, PAGE_PRESENT);

        /* Look up the newly created mapping */
        page = page_lookup(kernel_pml4, va, &entry);
        if (!page) {
            panic("no page mapped at VA %p after populate_region (i=%zu)", va, i);
        }
        if (!entry) {
            panic("PTE entry is NULL for VA %p (i=%zu)", va, i);
        }
        if (!(*entry & PAGE_PRESENT)) {
            panic("PTE for VA %p not marked PRESENT (entry=%lx, i=%zu)",
                  va, (unsigned long)*entry, i);
        }
        if (*entry & PAGE_HUGE) {
            panic("PTE for VA %p unexpectedly marked HUGE before full 2M is populated (i=%zu)",
                  va, i);
        }

        /* Also check the PDE at VA 0. It should not be merged yet */
        page = page_lookup(kernel_pml4, 0, &entry);
        if (!page) {
            panic("no page mapped at VA 0 after populating VA %p (i=%zu)", va, i);
        }
        if (!entry) {
            panic("PTE entry is NULL for VA 0 after populating VA %p (i=%zu)", va, i);
        }
        if (*entry & PAGE_HUGE) {
            panic("VA 0 became HUGE too early (i=%zu), merge happened before last 4K page", i);
        }
    }

    /* Populate the last 4KB page that completes the full 2MB range */
    populate_region(kernel_pml4,
        (void *)((PAGE_TABLE_ENTRIES - 1) * PAGE_SIZE),
        PAGE_SIZE,
        PAGE_PRESENT);

    /* Now the whole region should merge into one 2MB huge page */
    page = page_lookup(kernel_pml4, 0, &entry);
    if (!page) {
        panic("no page mapped at VA 0 after populating the last 4K page");
    }
    if (!entry) {
        panic("PTE entry is NULL for VA 0 after populating the last 4K page");
    }
    if (!(*entry & PAGE_PRESENT)) {
        panic("PTE for VA 0 not marked PRESENT after full 2M populate (entry=%lx)",
              (unsigned long)*entry);
    }
    if (!(*entry & PAGE_HUGE)) {
        panic("PTE for VA 0 not marked HUGE after full 2M populate (entry=%lx)",
              (unsigned long)*entry);
    }
    if (PAGE_ADDR(*entry) != page2pa(page)) {
        panic("PTE address for VA 0 (%lx) does not match page2pa(page) (%lx)",
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

    /* Clean everything again */
    unmap_page_range(kernel_pml4, 0, HPAGE_SIZE);

    /* Make sure unmapping worked */
    if (page_lookup(kernel_pml4, 0, NULL)) {
        panic("mapping at VA 0 still exists after final unmap");
    }
    if (kernel_pml4->entries[0] != 0) {
        panic("kernel_pml4->entries[0] not cleared after unmap, value=%lx",
              (unsigned long)kernel_pml4->entries[0]);
    }

    /* Verify no memory got leaked during the test */
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
