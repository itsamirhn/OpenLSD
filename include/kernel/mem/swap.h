#pragma once

#include <types.h>
#include <paging.h>
#include <task.h>

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
	// Keep the permissions and the dirty bit, but not the present bit
	return addr | (flags & (PAGE_UMASK | PAGE_DIRTY) & ~PAGE_PRESENT) | PAGE_SWAP;
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
physaddr_t *swap_lookup(struct task *task, void *va, size_t *npages);
void *swap_huge_partial(struct task *task, void *va, size_t len);

int swap_split(struct task *task, void *va);
void swap_wait(void *va, bool in_syscall);
void swap_out_range(void *va, size_t len);
void swap_cancel(struct task *task);
int swap_share(struct page_table *pml4, uintptr_t va, physaddr_t entry, size_t npages);
void swap_free(physaddr_t entry, size_t npages);
