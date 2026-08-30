#include <assert.h>
#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <paging.h>
#include <stdio.h>
#include <types.h>

extern void halt_kernel();
extern struct page_info *buddy_find(size_t req_order); 

static int run_test(struct probe_frame *frame) {
    struct page_info *page;
    physaddr_t pa;
    size_t order;
    
    cprintf("[TEST] Verifying physical alignment for orders 0-%d\n", BUDDY_MAX_ORDER - 1);

    for (order = 0; order < BUDDY_MAX_ORDER; order++) {
        page = buddy_find(order);
        if (page == NULL) continue;
		assert(!page->pp_free);

        pa = page2pa(page);
        size_t required_alignment = PAGE_SIZE * (1 << order);
        if ((pa % required_alignment) != 0) {
            panic("Error: Order %d page at %p is not correctly aligned!", order, pa);
        }

        page_free(page);
    }

    cprintf("[TEST] Alignment checks passed.\n");
    return __checksum__;
}

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = halt_kernel,
    .should_continue = false,
    .checksum = __checksum__,
};
