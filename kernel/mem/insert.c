
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct insert_info {
	struct page_table *pml4;
	struct page_info *page;
	uint64_t flags;
};

/* If the PTE already points to a present page, the reference count of the page
 * gets decremented and the TLB gets invalidated. Then this function increments
 * the reference count of the new page and sets the PTE to the new page with the
 * user-provided permissions.
 */
static int insert_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct insert_info *info = walker->udata;
	struct page_info *page;

	info->page->pp_ref++; // Because of same page re-insert, increment the reference count first

	if (*entry & PAGE_PRESENT) {
		page = pa2page(PAGE_ADDR(*entry));
		tlb_invalidate(info->pml4, (void *)base);
		page_decref(page);
		*entry = 0;
	}

	*entry = page2pa(info->page) | info->flags | PAGE_PRESENT;

	return 0;
}

/**
 * If the new page is a 4K page, we need to allocate a new page table or split
 * any existing huge page.
 *
 * Else, the new page is a 2M huge page. If the PDE already points to a present
 * huge page or another page table, we unmap all memory pointed to by the PDE.
 * Then, increment the reference count of the new huge page and set the PDE with
 * the user-provided permissions.
 *
 * Hint: do not use any of the unmap_* methods to unmap the existing memory, but
 * do so by hand instead. (Can you think of why?)
 */
static int insert_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct insert_info *info = walker->udata;
	struct page_info *page;
	struct page_table *table;

	info->page->pp_ref++; // Because of same page re-insert, increment the reference count first
	if (*entry & PAGE_PRESENT) {
		page = pa2page(PAGE_ADDR(*entry));
		if (*entry & PAGE_HUGE) {
			*entry = 0;
			tlb_invalidate(info->pml4, (void *)base);
			page_decref(page);
		} else {
			table = page2kva(page);
			for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) if (table->entries[i] & PAGE_PRESENT) {
					table->entries[i] = 0;
					tlb_invalidate(info->pml4, (void *)(base + (i * PAGE_SIZE)));
					page_decref(pa2page(PAGE_ADDR(table->entries[i])));
			}
			*entry = 0;
			tlb_invalidate(info->pml4, (void *)base);
			page_decref(page);
		}
	}

	*entry = page2pa(info->page) | info->flags | PAGE_PRESENT | PAGE_HUGE;

	return 0;
}

/* Map the physical page page at virtual address va. The flags argument
 * contains the permission to set for the PTE. The PAGE_PRESENT flag should
 * always be set.
 *
 * Requirements:
 *  - If there is already a (huge) page mapped at va, it should be removed using
 *    page_decref().
 *  - If necessary, a page should be allocated and inserted into the page table
 *    on demand. This can be done by providing ptbl_alloc() to the page walker.
 *  - The reference count of the page should be incremented upon a successful
 *    insertion of the page.
 *  - The TLB must be invalidated if a page was previously present at va.
 *  - A 4K page should not replace an entire 2M huge page.
 *
 * Corner-case hint: make sure to consider what happens when the same page is
 * re-inserted at the same virtual address in the same page table. However, do
 * not try to distinguish this case in your code, as this frequently leads to
 * subtle bugs. There is another elegant way to handle everything in the same
 * code path.
 *
 * Hint: what should happen when the user inserts a 2M huge page at a
 * misaligned address?
 *
 * Hint: how do you deal with transparent huge paging in this method?
 *
 * Hint: this function calls walk_page_range(), hpage_aligned(), and page2pa().
 */
int page_insert(struct page_table *pml4, struct page_info *page, void *va,
    uint64_t flags)
{
	struct insert_info info = {
		.pml4 = pml4,
		.page = page,
		.flags = flags,
	};
	struct page_walker walker = {
		.pte_callback = insert_pte,
		.pde_callback = insert_pde,
		.pml4e_callback = ptbl_alloc,
		.pdpte_callback = ptbl_alloc,
		.udata = &info,
	};
	
	if (!(flags & PAGE_PRESENT)) return -1;

	if (page->pp_order == BUDDY_2M_PAGE && (flags & PAGE_HUGE) && hpage_aligned((uintptr_t)va)) {
		return walk_page_range(pml4, va, va + HPAGE_SIZE, &walker);
	} else if (page->pp_order == BUDDY_4K_PAGE && !(flags & PAGE_HUGE)) {
		walker.pde_callback = ptbl_split;
		walker.pde_unmap = ptbl_merge;
		return walk_page_range(pml4, va, va + PAGE_SIZE, &walker);
	}
	return -1;
}
