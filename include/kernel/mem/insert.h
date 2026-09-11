
#pragma once

#include <types.h>
#include <paging.h>

int page_insert(struct page_table *pml4, struct page_info *page, void *va,
    uint64_t flags);

#ifdef BONUS_OUT_OF_BOUNDS
int page_insert_guarded(struct page_table *pml4, struct page_info *page, void *va,
	uint64_t flags);
#endif
