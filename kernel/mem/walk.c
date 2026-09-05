
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

/* Given an address addr, this function returns the sign extended address. */
static uintptr_t sign_extend(uintptr_t addr)
{
	return (addr < USER_LIM) ? addr : (0xffff000000000000ull | addr);
}

/* Given an addresss addr, this function returns the page boundary. */
static uintptr_t ptbl_end(uintptr_t addr)
{
	return addr | (PAGE_SIZE - 1);
}

static uintptr_t ptbl_start(uintptr_t addr)
{
	return addr & ~(PAGE_SIZE - 1);
}

/* Given an address addr, this function returns the page table boundary. */
static uintptr_t pdir_end(uintptr_t addr)
{
	return addr | (PAGE_TABLE_SPAN - 1);
}

static uintptr_t pdir_start(uintptr_t addr)
{
	return addr & ~(PAGE_TABLE_SPAN - 1);
}

/* Given an address addr, this function returns the page directory boundary. */
static uintptr_t pdpt_end(uintptr_t addr)
{
	return addr | (PAGE_DIR_SPAN - 1);
}

static uintptr_t pdpt_start(uintptr_t addr)
{
	return addr & ~(PAGE_DIR_SPAN - 1);
}

/* Given an address addr, this function returns the PDPT boundary. */
static uintptr_t pml4_end(uintptr_t addr)
{
	return addr | (PDPT_SPAN - 1);
}

static uintptr_t pml4_start(uintptr_t addr)
{
	return addr & ~(PDPT_SPAN - 1);
}

/* Walks over the page range from base to end iterating over the entries in the
 * given page table ptbl. The user may provide walker->pte_callback() that gets
 * called for every entry in the page table. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * page table.
 *
 * Hint: this function calls ptbl_end() and ptbl_start to get the boundaries of
 * the current page.
 * Hint: the next page is at ptbl_end() + 1.
 * Hint: the loop condition is next < end.
 */
static int ptbl_walk_range(struct page_table *ptbl, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	if (end < base) return 0;

	uint32_t i;
	int ret;
	uintptr_t curr_base, curr_end;
	physaddr_t * entry;

	for (i = PAGE_TABLE_INDEX(base); i <= PAGE_TABLE_INDEX(end) && base < end; ++i) {
		entry = &ptbl->entries[i];
		curr_base = ptbl_start(base);
		curr_end = ptbl_end(base);

		if (walker->pte_callback != NULL) {
			ret = walker->pte_callback(entry, curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		if (walker->pt_hole_callback != NULL && !(*entry & PAGE_PRESENT)) {
			ret = walker->pt_hole_callback(curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		base = curr_end + 1;
	}

	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given page directory pdir. The user may provide walker->pde_callback() that
 * gets called for every entry in the page directory. In addition the user may
 * provide walker->pt_hole_callback() that gets called for every unmapped entry
 * in the page directory. If the PDE is present, but not a huge page, this
 * function calls ptbl_walk_range() to iterate over the entries in the page
 * table. The user may provide walker->pde_unmap() that gets called for every
 * present PDE after walking over the page table.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pdir_walk_range(struct page_table *pdir, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	uint32_t i;
	int ret;
	uintptr_t curr_base, curr_end;
	physaddr_t entry;

	struct page_table *ptbl;

	for(i = PAGE_DIR_INDEX(base); i <= PAGE_DIR_INDEX(end) && base < end; ++i){
		entry = pdir->entries[i];
		curr_base = MAX(base, pdir_start(base));
		curr_end = entry & PAGE_HUGE ? pdir_end(base) : MIN(end, pdir_end(base));
		if(walker->pde_callback != NULL){
			ret = walker->pde_callback(&pdir->entries[i], curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		if(PAGE_PRESENT & entry){
			if(!(PAGE_HUGE & entry)){
				ptbl = KADDR(PAGE_ADDR(entry));
				ptbl_walk_range(ptbl, curr_base, curr_end, walker);
				if(walker->pde_unmap != NULL){
					ret = walker->pde_unmap(&pdir->entries[i], curr_base, curr_end, walker);
					if (ret < 0) {
						return ret;
					}
				}	
		}
		}else if(walker->pt_hole_callback != NULL){
			ret = walker->pt_hole_callback(curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		base = curr_end + 1;
	}
	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given PDPT pdpt. The user may provide walker->pdpte_callback() that gets
 * called for every entry in the PDPT. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * PDPT. If the PDPTE is present, this function calls pdir_walk_range() to
 * iterate over the entries in the page directory. The user may provide
 * walker->pdpte_unmap() that gets called for every present PDPTE after walking
 * over the page directory.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pdpt_walk_range(struct page_table *pdpt, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	uint32_t i;
	int ret;
	uintptr_t curr_base, curr_end;
	physaddr_t entry;

	struct page_table *pdir;

	for(i = PDPT_INDEX(base); i <= PDPT_INDEX(end) && base < end; ++i){
		entry = pdpt->entries[i];
		curr_base = MAX(base, pdpt_start(base));
		curr_end = MIN(end, pdpt_end(base));
		if(walker->pdpte_callback != NULL){
			ret = walker->pdpte_callback(&pdpt->entries[i], curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		if(PAGE_PRESENT & entry){
			pdir = KADDR(PAGE_ADDR(entry));
			pdir_walk_range(pdir, curr_base, curr_end, walker);
			if(walker->pdpte_unmap != NULL){
				ret = walker->pdpte_unmap(&pdpt->entries[i], curr_base, curr_end, walker);
				if (ret < 0) {
					return ret;
				}
			}	
		}else if(walker->pt_hole_callback != NULL){
			ret = walker->pt_hole_callback(curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		base = curr_end + 1;
	}
	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given PML4 pml4. The user may provide walker->pml4e_callback() that gets
 * called for every entry in the PML4. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * PML4. If the PML4E is present, this function calls pdpt_walk_range() to
 * iterate over the entries in the PDPT. The user may provide
 * walker->pml4e_unmap() that gets called for every present PML4E after walking
 * over the PDPT.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pml4_walk_range(struct page_table *pml4, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint32_t i;
	int ret;
	uintptr_t curr_base, curr_end;
	physaddr_t entry;

	struct page_table *pdpt;

	for(i = PML4_INDEX(base); i <= PML4_INDEX(end) && base < end; ++i){
		entry = pml4->entries[i];
		curr_base = sign_extend(MAX(base, pml4_start(base)));
		curr_end = sign_extend(MIN(end, pml4_end(base)));
		if(walker->pml4e_callback != NULL){
			ret = walker->pml4e_callback(&pml4->entries[i], curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		if(PAGE_PRESENT & entry){
			pdpt = KADDR(PAGE_ADDR(entry));

			pdpt_walk_range(pdpt, curr_base, curr_end, walker);
			if(walker->pml4e_unmap != NULL){
				ret = walker->pml4e_unmap(&pml4->entries[i], curr_base, curr_end, walker);
				if (ret < 0) {
					return ret;
				}
			}	
		}else if(walker->pt_hole_callback != NULL){
			ret = walker->pt_hole_callback(curr_base, curr_end, walker);
			if (ret < 0) {
				return ret;
			}
		}
		base = curr_end + 1;
	}
	return 0;
}

/* Helper function to walk over a page range starting at base and ending before
 * end.
 */
int walk_page_range(struct page_table *pml4, void *base, void *end,
	struct page_walker *walker)
{
	return pml4_walk_range(pml4, ROUNDDOWN((uintptr_t)base, PAGE_SIZE),
		ROUNDUP((uintptr_t)end, PAGE_SIZE) - 1, walker);
}

/* Helper function to walk over all pages. */
int walk_all_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, 0, KERNEL_LIM, walker);
}

/* Helper function to walk over all user pages. */
int walk_user_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, 0, USER_LIM - 1, walker);
}

/* Helper function to walk over all kernel pages. */
int walk_kernel_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, KERNEL_VMA, KERNEL_LIM, walker);
}
