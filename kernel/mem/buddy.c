#include <types.h>
#include <list.h>
#include <paging.h>
#include <spinlock.h>
#include <string.h>

#include <kernel/mem.h>

/* Physical page metadata. */
size_t npages;
struct page_info *pages;

/*
 * List of free buddy chunks (often also referred to as buddy pages or simply
 * pages). Each order has a list containing all free buddy chunks of the
 * specific buddy order. Buddy orders go from 0 to BUDDY_MAX_ORDER - 1
 */
struct list buddy_free_list[BUDDY_MAX_ORDER];

#ifndef USE_BIG_KERNEL_LOCK
/* Lock for the buddy allocator. */
struct spinlock buddy_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "buddy_lock",
#endif
};
#endif

/* Counts the number of free pages for the given order.
 */
size_t count_free_pages(size_t order)
{
	struct list *node;
	size_t nfree_pages = 0;

	if (order >= BUDDY_MAX_ORDER) {
		return 0;
	}

	list_foreach(buddy_free_list + order, node) {
		++nfree_pages;
	}

	return nfree_pages;
}

/* Shows the number of free pages in the buddy allocator as well as the amount
 * of free memory in kiB.
 *
 * Use this function to diagnose your buddy allocator.
 */
void show_buddy_info(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	cprintf("Buddy allocator:\n");

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);

		cprintf("  order #%u pages=%u\n", order, nfree_pages);

		nfree += nfree_pages * (1 << (order + 12));
	}

	cprintf("  free: %u kiB\n", nfree / 1024);
}

/* Gets the total amount of free pages. */
size_t count_total_free_pages(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);
		nfree += nfree_pages * (1 << order);
	}

	return nfree;
}

/* Splits lhs into free pages until the order of the page is the requested
 * order req_order.
 *
 * The algorithm to split pages is as follows:
 *  - Given the page of order k, locate the page and its buddy at order k - 1.
 *  - Decrement the order of both the page and its buddy.
 *  - Mark the buddy page as free and add it to the free list.
 *  - Repeat until the page is of the requested order.
 *
 * Returns a page of the requested order.
 */
struct page_info *buddy_split(struct page_info *lhs, size_t req_order)
{
	if (lhs == NULL) {
		return NULL;
	}

	assert(req_order <= lhs->pp_order);
	if (req_order == lhs->pp_order) {
		return lhs;
	}

	assert(lhs->pp_order > 0);
	size_t new_order = lhs->pp_order - 1;
	struct page_info *buddy = pa2page(BUDDY_PA(page2pa(lhs), new_order));
	assert(buddy != NULL);
	assert(buddy->pp_free == 0);
	buddy->pp_free = 1;
	lhs->pp_order = buddy->pp_order = new_order;

	list_add_tail(buddy_free_list + new_order, &buddy->pp_node);

	return buddy_split(lhs, req_order);
}

/* Merges the buddy of the page with the page if the buddy is free to form
 * larger and larger free pages until either the maximum order is reached or
 * no free buddy is found.
 *
 * The algorithm to merge pages is as follows:
 *  - Given the page of order k, locate the page with the lowest address
 *    and its buddy of order k.
 *  - Check if both the page and the buddy are free and whether the order
 *    matches.
 *  - Remove the page and its buddy from the free list.
 *  - Increment the order of the page.
 *  - Repeat until the maximum order has been reached or until the buddy is not
 *    free.
 *
 * Returns the largest merged free page possible.
 */
struct page_info *buddy_merge(struct page_info *page)
{
	assert(page->pp_free == 1);
	assert(page->pp_order < BUDDY_MAX_ORDER);
	if (page->pp_order == BUDDY_MAX_ORDER - 1) {
		list_add_tail(buddy_free_list + page->pp_order, &page->pp_node);
		return page;
	}

	struct page_info *buddy = pa2page(BUDDY_PA(page2pa(page), page->pp_order));
	assert(buddy != NULL);
	if (buddy->pp_free == 0 || buddy->pp_order != page->pp_order) {
		list_add_tail(buddy_free_list + page->pp_order, &page->pp_node);
		return page;
	}
	assert(buddy->pp_order == page->pp_order);
	assert(buddy->pp_free == 1);

	list_del(&buddy->pp_node);

	if (page2pa(page) < page2pa(buddy)) {
		page->pp_order++;
		buddy->pp_free = 0;
		#ifdef BONUS_INVALID_FREE
			buddy->pp_order = BUDDY_MAX_ORDER;
		#endif
		return buddy_merge(page);
	} else {
		buddy->pp_order++;
		page->pp_free = 0;
		#ifdef BONUS_INVALID_FREE
			page->pp_order = BUDDY_MAX_ORDER;
		#endif
		return buddy_merge(buddy);
	}
}

/* Given the order req_order, attempts to find a page of that order or a larger
 * order in the free list. In case the order of the free page is larger than the
 * requested order, the page is split down to the requested order using
 * buddy_split().
 *
 * Returns a page of the requested order or NULL if no such page can be found.
 */
struct page_info *buddy_find(size_t req_order)
{
	if (req_order >= BUDDY_MAX_ORDER) {
		return NULL;
	}

	if (list_is_empty(buddy_free_list + req_order)) {
		return buddy_split(buddy_find(req_order + 1), req_order);
	}

	struct page_info *page = container_of(list_pop(buddy_free_list + req_order), struct page_info, pp_node);
	assert(page->pp_free);
	assert(page->pp_order == req_order);
	page->pp_free = 0;

	return page;
}

/*
 * Allocates a physical page.
 *
 * if (alloc_flags & ALLOC_ZERO), fills the entire returned physical page with
 * '\0' bytes.
 * if (alloc_flags & ALLOC_HUGE), returns a huge physical 2M page.
 *
 * Beware: this function does NOT increment the reference count of the page -
 * this is the caller's responsibility.
 *
 * Returns NULL if out of free memory.
 *
 * Hint: use buddy_find() to find a free page of the right order.
 * Hint: use page2kva() and memset() to clear the page.
 */
struct page_info *page_alloc(int alloc_flags)
{
	int order = BUDDY_4K_PAGE;
	
	if (alloc_flags & ALLOC_HUGE) {
		order = BUDDY_2M_PAGE;
	}

	struct page_info *page = buddy_find(order);
	
	#if defined(BONUS_USE_AFTER_FREE) || defined(BONUS_OUT_OF_BOUNDS)
	if(page != NULL){
		long *va = page2kva(page);
		for(int i = 0; i < PAGE_SIZE / sizeof(long); i++) {
			if (va[i] != 0 && va[i] != CANARY) {
				panic("Use after free detected for page %p", page2pa(page));
			}
		}
	}
	#endif

	if	(page != NULL && (alloc_flags & ALLOC_ZERO)) {
		memset(page2kva(page), 0, BUDDY_SIZE(order));
	}
	
	return page;
}

/*
 * Return a page to the free list.
 * This function should only be called when pp->pp_ref reaches 0.
 * Pages can only be freed when they are available.
 *
 * Hint: mark the page as free and use buddy_merge() to merge the free page
 * with its buddies before returning the page to the free list.
 *
 * Note: this method should typically not be called directly. Use page_decref
 * instead, to properly handle refcounting.
 */
void page_free(struct page_info *pp)
{
	#ifdef BONUS_INVALID_FREE
		if (!(pages <= pp && pp < pages + npages)) {
				panic("Invalid free detected; %p is outside the pages array", pp);
		}

		if (((void *)pp - (void *)pages) % sizeof *pages) {
				panic("Invalid free detected; %p is not page_info-aligned", pp);
		}
		
		if (pp->pp_order >= BUDDY_MAX_ORDER) {
				panic("Invalid free detected; %p is not a block header", pp);
		}
	#endif

	#ifdef BONUS_DOUBLE_FREE
		if (pp->pp_free == 1) {
			panic("Double free detected for page %p", page2pa(pp));
		}
	#endif

	#if defined(BONUS_USE_AFTER_FREE) || defined(BONUS_OUT_OF_BOUNDS)
		long *va = page2kva(pp);
		for (int i = 0; i < PAGE_SIZE / sizeof(long); i++) {
			va[i] = CANARY;
		}
	#endif

	assert(pp->pp_ref == 0);
	pp->pp_free = 1;

	buddy_merge(pp);
}

/*
 * Decrement the reference count on a page, freeing it if there
 * are no more refs.
 */
void page_decref(struct page_info *pp)
{
	// Sanity check to help catch some sneaky bugs
	assert(pp->pp_ref > 0);
	if (--pp->pp_ref == 0) {
		page_free(pp);
	}
}

static int in_page_range(void *p)
{
	return ((uintptr_t)pages <= (uintptr_t)p &&
	        (uintptr_t)p < (uintptr_t)(pages + npages));
}

static void *update_ptr(void *p)
{
	if (!in_page_range(p))
		return p;

	return (void *)((uintptr_t)p + KPAGES - (uintptr_t)pages);
}

void buddy_migrate(void)
{
	struct page_info *page;
	struct list *node;
	size_t i;

	for (i = 0; i < npages; ++i) {
		page = pages + i;
		node = &page->pp_node;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	for (i = 0; i < BUDDY_MAX_ORDER; ++i) {
		node = buddy_free_list + i;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	pages = (struct page_info *)KPAGES;
}

/**
 * Increase the range of pages managed by our kernel in the pages[] array. This
 * method will increase npages by chunks of size <the number of pages in a
 * max_order page>.
 *
 * This method expects a size parameter representing the requested minimum size
 * of npages. npages is then increased to the first multiple of the chunk size
 * strictly larger than size, so that pages[size] is a valid page.
 */
int buddy_grow(struct page_table *pml4, size_t size)
{
	// We grow the page scope of the buddy allocator by an entire MAX_ORDER
	// worth of pages every time. Compute how many pages this corresponds to,
	// and how many pages are needed to store that many page_info structs.
	size_t increment_structs = (1 << (12 + BUDDY_MAX_ORDER - 1)) / PAGE_SIZE;
	size_t increment_pages = ROUNDUP(increment_structs * sizeof(struct page_info), PAGE_SIZE) / PAGE_SIZE;

	// Continuously grow the buddy allocator while the current size is less
	// than the requested size.
	while(npages <= size) {
		struct page_info *pages_end = pages + npages;

		// Allocate pages for the new structs
		for (size_t i = 0; i < increment_pages; i++) {
			struct page_info *page = page_alloc(ALLOC_ZERO);
			if (!page)
				return -1; // We ran out of memory

			// Ensure the page is mapped in the correct location: after the
			// existing pages array
			int ret = page_insert(pml4, page, (char *) pages_end + i * PAGE_SIZE,
			    PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
			if (ret < 0)
				return ret;
		}

		// Initialise newly allocated page_info structs. Crucially, we do NOT
		// hand these new pages to the buddy allocator through page_free, since
		// we cannot know whether they are actually free or available. This is
		// the job of page_init_ext().
		for(size_t i = 0; i < increment_structs; i++) {
			struct page_info *info = pages_end + i;
			list_init(&info->pp_node);
		}

		// Update npages size
		npages += increment_structs;
	}

	return 0;
}
