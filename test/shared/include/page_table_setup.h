#pragma once

#include <paging.h>
#include <kernel/mem.h>

// Struct & enum to store convenient references to paging structures.
enum page_level {
	PML4 = 0,
	PDPT = 1,
	PDIR = 2,
	PTE = 3,
};

struct paging_info {
	struct page_table *tables[PTE + 1];
	struct page_info *pages[PTE + 1];
};

/**
 * Returns the table entry for va in the provided pml4 at level lvl.
 * Verifies PAGE_PRESENT for each entry up to lvl, stops short if a page is marked as PAGE_HUGE.
 *
 * @param pml4 A reference to the pml4 to check in
 * @param va Virtual address to check
 * @param lvl The paging level to check until
 * @return A valid entry with PAGE_PRESENT on success, 0 on failure
 */
physaddr_t get_entry(struct page_table *pml4, uintptr_t va, enum page_level lvl);

/**
 * Allocates and sets up the initial PML4 page table entry in the given PD.
 * If the entry already exists, reuses the existing PML4.
 * It allocates no actual "usable" memory, only whats needed for the paging hierarchy.
 *
 * @param pml4 Where a reference to the created pml4 will be stored. A reference will also be stored in info if present
 * @param (optional) info Structure holding references to page_table and page_info for this level
 * @return 0 on success, -1 on failure
 */
int setup_pml4(struct page_table **pml4, struct paging_info *info);

/**
 * Allocates and sets up a PDPT, PD, or PT table depending on lvl.
 * If the entry already exists, it reuses the existing entry.
 *
 * @param va Virtual address to determine the index in the table
 * @param lvl The level of table to initialize
 * @param parent The parent table to insert the new entry into
 * @param (optional) info Structure holding references to page_table and page_info for this level
 * @return 0 on success, -1 on failure
 */
int setup_table(uintptr_t va, enum page_level lvl, struct page_table *parent, struct paging_info *info);

/**
 * Allocates and sets up a complete page table hierarchy (PML4, PDPT, PD, (PT)) for a given virtual address.
 * This is a convenience function that calls setup_pml4, setup_pdpt, setup_pd, and setup_pt in sequence.
 * It allocates no actual "usable" memory, only whats needed for the paging hierarchy.
 *
 * @param pml4 Where a reference to the created pml4 will be stored. A reference will also be stored in info if present
 * @param va Virtual address to determine the indices
 * @param final_lvl The final paging level that should be created. Allows partial table creation for tests
 * @param (optional) info Structure holding references to page_table and page_info for this level
 * @return 0 on success, -1 on failure
 */
int setup_page_tables(struct page_table **pml4, uintptr_t va, enum page_level final_lvl, struct paging_info *info);

