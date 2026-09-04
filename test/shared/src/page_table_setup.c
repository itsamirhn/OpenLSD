#include <string.h>

#include "page_table_setup.h"

char *lvl_strings[] = {
    [PML4] = "PML4",
    [PDPT] = "PDPT",
    [PDIR] = "PDIR",
    [PTE] = "PTE",
};

static inline size_t lvl_to_idx(uintptr_t va, enum page_level lvl)
{
	return ((va) >> (12 + ((3 - (uint32_t)lvl) * 9))) & PAGE_TABLE_MASK; // All the masks are the same
}

static physaddr_t get_entry_rec(struct page_table *parent, uintptr_t va, enum page_level tgt_lvl, enum page_level cur_lvl)
{
	size_t idx = lvl_to_idx(va, cur_lvl);

	// Check if the entry needed for this va is present
	if (!(parent->entries[idx] & PAGE_PRESENT)) {
		cprintf("Missing %s entry for va %p\n", lvl_strings[cur_lvl], (void *)va);
		return 0;
	}

	// Stop short if the current page is huge
	// Stop also if we have reached desired layer
	if ((parent->entries[idx] & PAGE_HUGE) || (cur_lvl == tgt_lvl))
		return parent->entries[idx];

	return get_entry_rec(KADDR(PAGE_ADDR(parent->entries[idx])), va, tgt_lvl, cur_lvl + 1);
}

physaddr_t get_entry(struct page_table *pml4, uintptr_t va, enum page_level lvl)
{
	return get_entry_rec(pml4, va, lvl, PML4);
}

int setup_table(uintptr_t va, enum page_level lvl, struct page_table *parent, struct paging_info *info)
{
	size_t idx = lvl_to_idx(va, lvl - 1); // Needs to index into the parent
	struct page_info *page;
	struct page_table *table;

	if (!parent || lvl <= PML4) {
		return -1;
	}

	if (parent->entries[idx] & PAGE_PRESENT) {
		/* Reuse existing pageing */
		page = pa2page(PAGE_ADDR(parent->entries[idx]));
		table = KADDR(PAGE_ADDR(parent->entries[idx]));
	} else {
		/* Allocate new page */
		page = page_alloc(ALLOC_ZERO);
		if (!page)
			return -1;

		page->pp_ref++;
		table = page2kva(page);

		parent->entries[idx] = page2pa(page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	}

	if (info) {
		info->pages[lvl] = page;
		info->tables[lvl] = table;
	}

	return 0;
}

int setup_pml4(struct page_table **pml4, struct paging_info *info)
{
	struct page_info *pml4_page;

	if (pml4 == NULL)
		return -1;

	if (!*pml4) {
		pml4_page = page_alloc(ALLOC_ZERO);
		if (!pml4_page)
			return -1;

		pml4_page->pp_ref++;
		*pml4 = page2kva(pml4_page);
	} else {
		// No reference to the pa of the provided pml4 are are available here
		// as we only have the kva of the data page
		pml4_page = NULL;
	}

	if (info) {
		info->tables[PML4] = *pml4;
		info->pages[PML4] = pml4_page;
	}

	return 0;
}

int setup_page_tables(struct page_table **pml4, uintptr_t va, enum page_level final_lvl, struct paging_info *info)
{
	struct paging_info _info;

	// Minimum level is pml4, so this always runs
	if (setup_pml4(pml4, &_info) < 0) {
		return -1;
	}

	if (final_lvl >= PDPT && setup_table(va, PDPT, _info.tables[PML4], &_info) < 0) {
		return -1;
	}

	if (final_lvl >= PDIR && setup_table(va, PDIR, _info.tables[PDPT], &_info) < 0) {
		return -1;
	}

	if (final_lvl >= PTE && setup_table(va, PTE, _info.tables[PDIR], &_info) < 0) {
		return -1;
	}

	// Copy over private _info, if info is present
	if (info) {
		memcpy(info, &_info, sizeof(struct paging_info));
	}

	return 0;
}

