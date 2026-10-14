#pragma once

#include <types.h>
#include <paging.h>
#include <task.h>

#define SWAP_FLAGS (PAGE_WRITE | PAGE_USER | PAGE_NO_EXEC | PAGE_DIRTY)

static inline bool pte_swapped(physaddr_t entry)
{
	return !(entry & PAGE_PRESENT) && (entry & PAGE_SWAP);
}


static inline physaddr_t swap_pte(size_t slot, physaddr_t flags)
{
	physaddr_t addr = (physaddr_t)slot << PAGE_TABLE_SHIFT;

#ifdef BONUS_L1TF_MDS
	addr = PAGE_INVERT(addr);
#endif
	return addr | (flags & SWAP_FLAGS) | PAGE_SWAP;
}

static inline size_t swap_slot(physaddr_t entry)
{
#ifdef BONUS_L1TF_MDS
	return PAGE_INVERT(entry) >> PAGE_TABLE_SHIFT;
#else
	return PAGE_ADDR(entry) >> PAGE_TABLE_SHIFT;
#endif
}

void swap_init(void);
bool swap_enabled(void);
int swap_share(struct page_table *pml4, uintptr_t va, physaddr_t entry);
void swap_free(physaddr_t entry);
