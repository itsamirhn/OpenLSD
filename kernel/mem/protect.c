
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct protect_info {
	struct page_table *pml4;
	uint64_t flags;
	uintptr_t base, end;
};

/* Changes the protection of the page. Avoid calling tlb_invalidate() if
 * nothing changes at all.
 */
static int protect_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct protect_info *info = walker->udata;

	if (!(*entry & PAGE_PRESENT)) return 0;

	physaddr_t nentry = PAGE_ADDR(*entry) | (info->flags) | PAGE_PRESENT | (*entry & PAGE_DIRTY);
	
	if (nentry == *entry) return 0;
	
	*entry = nentry;
	tlb_invalidate(info->pml4, (void *)base);

	return 0;
}

/* Changes the protection of the huge page, if the page is a huge page and if
 * the range covers the full huge page. Otherwise if the page is a huge page,
 * but if the range does not span an entire huge page, this function calls
 * ptbl_split() to split up the huge page. Avoid calling tlb_invalidate() if
 * nothing changes at all.
 */
static int protect_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct protect_info *info = walker->udata;

	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_HUGE)) return 0;
	if (info->base <= base && end <= info->end) {
		physaddr_t nentry = PAGE_ADDR(*entry) | (info->flags) | PAGE_PRESENT | PAGE_HUGE | (*entry & PAGE_DIRTY); 
		if (nentry == *entry) return 0;
		*entry = nentry;
		tlb_invalidate(info->pml4, (void *)base);
		return 0;
	}

	return ptbl_split(entry, base, end, walker);
}

/* Changes the protection of the region [va, va + size) to the permissions
 * specified by flags.
 */
void protect_region(struct page_table *pml4, void *va, size_t size,
    uint64_t flags)
{
	struct protect_info info = {
		.pml4 = pml4,
		.flags = flags,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = protect_pte,
		.pde_callback = protect_pde,
		.pde_unmap = ptbl_merge,
		.udata = &info,
	};

	walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker);
}
