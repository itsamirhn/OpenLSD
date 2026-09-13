
#include "error.h"
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct populate_info {
	uint64_t flags;
	uintptr_t base, end;
};

static int populate_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct page_info *page;
	struct populate_info *info = walker->udata;

	if (*entry & PAGE_PRESENT) return 0;

	page = page_alloc(ALLOC_ZERO);
	if (page == NULL) return -ENOMEM;
	page->pp_ref++;
	*entry = page2pa(page) | (info->flags & ~(uint64_t)PAGE_HUGE) | PAGE_PRESENT;

	return 0;
}

static int populate_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct page_info *page;
	struct populate_info *info = walker->udata;

	if (*entry & PAGE_PRESENT && *entry & PAGE_HUGE) return 0;
	if (!(*entry & PAGE_PRESENT) && info->base <= base && end <= info->end) {
		page = page_alloc(ALLOC_ZERO | ALLOC_HUGE);
		if (page == NULL) return -ENOMEM;
		page->pp_ref++;
		*entry = page2pa(page) | (info->flags) | PAGE_PRESENT | PAGE_HUGE;
		return 0;
	}

	return ptbl_alloc(entry, base, end, walker);
}

/* Populates the region [va, va + size) with pages by allocating pages from the
 * frame allocator and mapping them.
 */
void populate_region(struct page_table *pml4, void *va, size_t size,
	uint64_t flags)
{
	struct populate_info info = {
		.flags = flags,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = populate_pte,
		.pde_callback = populate_pde,
		.pdpte_callback = ptbl_alloc,
		.pml4e_callback = ptbl_alloc,
		.udata = &info,
	};

	walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker);
}
