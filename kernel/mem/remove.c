
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct remove_info {
	struct page_table *pml4;
	uintptr_t base, end;
};

/* Removes the page if present by decrementing the reference count, clearing the
 * PTE and invalidating the TLB.
 */
static int remove_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct remove_info *info = walker->udata;
	struct page_info *page;

	if (!(*entry & PAGE_PRESENT)) return 0;

	page = pa2page(PAGE_ADDR(*entry));
	*entry = 0;
	tlb_invalidate(info->pml4, (void *)base);
	page_decref(page);

	return 0;
}

/* Removes the page if present and if it is a huge page by decrementing the
 * reference count, clearing the PDE and invalidating the TLB.
 * If the region to remove does not span the entire PDE, perform a split.
 *
 * Hint: How are you going to identify the intended region to remove? What options
 * are available to you to provide extra information to this method?
 */
static int remove_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct remove_info *info = walker->udata;
	struct page_info *page;

	if (!(*entry & PAGE_PRESENT)) return 0;
	if (!(*entry & PAGE_HUGE)) return 0;
	
	if (info->base <= base && end <= info->end) {
		page = pa2page(PAGE_ADDR(*entry));
		*entry = 0;
		tlb_invalidate(info->pml4, (void *)base);
		page_decref(page);
		return 0;
	}
	
	return ptbl_split(entry, base, end, walker);	
}


/* Unmaps the range of pages from [va, va + size). */
void unmap_page_range(struct page_table *pml4, void *va, size_t size)
{
	struct remove_info info = {
		.pml4 = pml4,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = remove_pte,
		.pde_callback = remove_pde,
		.pde_unmap = ptbl_free,
		.pdpte_unmap = ptbl_free,
		.pml4e_unmap = ptbl_free,
		.udata = &info,
	};

	assert(walk_page_range(pml4, va, va + size, &walker) == 0);
}

/* Unmaps all user pages. */
void unmap_user_pages(struct page_table *pml4)
{
	unmap_page_range(pml4, 0, USER_LIM);
}

/* Unmaps the physical page at the virtual address va. */
void page_remove(struct page_table *pml4, void *va)
{
	struct page_info *page = page_lookup(pml4, va, NULL);
	#ifdef BONUS_PAGING_INVALID_FREE
		if(!page_aligned((uintptr_t) va)) {
			panic("Invalid page removal at misaligned virtual address %p", va);
		}
	#endif
	#if defined(BONUS_DOUBLE_FREE) || defined(BONUS_PAGING_INVALID_FREE)
		if (!page) {
			panic("Invalid page removal at virtual address %p", va);
		}
	#endif
	if (page) {
		if (page->pp_order == BUDDY_4K_PAGE) unmap_page_range(pml4, va, PAGE_SIZE);
		else unmap_page_range(pml4, va, HPAGE_SIZE);
	}
}
