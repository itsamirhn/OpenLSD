#include <types.h>
#include <assert.h>
#include <atomic.h>
#include <error.h>
#include <list.h>
#include <paging.h>
#include <string.h>
#include <task.h>
#include <cpu.h>

#include <kernel/dev/disk.h>
#include <kernel/mem.h>
#include <kernel/sched.h>

#define SWAP_MAGIC "SWAP"
#define SWAP_MAX_SLOTS (128 * 1024 * 1024 / PAGE_SIZE)
#define SWAP_BATCH 64
// MAX Pages one AHCI can process
#define SWAP_CLUSTER 16

#define SYSCALL_INSN_SIZE 2

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif
extern pid_t pid_max;
extern struct task **tasks;

#ifndef USE_BIG_KERNEL_LOCK
static struct spinlock swap_lock = {
	.rank = RANK_SWAP,
#ifdef DEBUG_SPINLOCK
	.name = "swap_lock",
#endif
};
#endif

static struct disk *swap_disk;
static size_t sectors_per_page;

static uint16_t swap_map[SWAP_MAX_SLOTS];
static size_t nslots, swap_cursor;

// The CLOCK hand, sweeping over the pages of all tasks
static struct {
	pid_t pid;
	uintptr_t va;
} hand;

// Tasks waiting on swapd, linked through task_node
static struct list swap_waiters = LIST_INIT(swap_waiters);
static struct task *swapd_task;

static struct page_info *bounce; // copy page, huge page

// The disk state, a read pages in task_swap_va of the task
static struct {
	enum { IO_NONE, IO_READ, IO_WRITE } op;
	struct page_info *page;
	size_t slot, npages, done;
	struct task *task;
} io;

// Nothing left to evict, so OOM kill once swap_lock is released
static bool swapd_oom;

// Pages evicted so far for the task at the head that needs memory
static size_t swapd_batch;

static size_t io_chunk(void)
{
	return MIN(io.npages - io.done, SWAP_CLUSTER);
}

// Issue command and poll disk
static int swap_rw(void)
{
	void *buf = page2kva(io.page + io.done);
	uint64_t sector = (io.slot + io.done) * sectors_per_page;
	size_t count = io_chunk() * sectors_per_page;
	int64_t ret;

	ret = io.op == IO_WRITE ? disk_write(swap_disk, buf, count, sector) : disk_read(swap_disk, buf, count, sector);

	if (ret == -EAGAIN) return ret;
	return ret == io_chunk() * PAGE_SIZE ? 0 : -EFAULT;
}

static void swapd(void *arg);

// Find swap disk
void swap_init(void)
{
	struct disk_stat stat;
	size_t i;
	int ret;

	if (!(bounce = page_alloc(ALLOC_HUGE))) return;
	atomic_inc(&bounce->pp_ref);

	io.op = IO_READ;
	io.page = bounce;
	io.slot = 0;
	io.npages = 1;
	io.done = 0;

	for (i = 0; i < ndisks && !swap_disk; ++i) {
		if (disk_stat(disks[i], &stat) < 0) continue;

		swap_disk = disks[i];
		sectors_per_page = PAGE_SIZE / stat.sect_size;

		while ((ret = swap_rw()) == -EAGAIN);
		if (ret < 0 || memcmp(page2kva(bounce), SWAP_MAGIC, strlen(SWAP_MAGIC)) != 0) swap_disk = NULL;
	}

	io.op = IO_NONE;
	if (!swap_disk) return;

	nslots = MIN(stat.nsectors / sectors_per_page, SWAP_MAX_SLOTS);
	cprintf("SWAP: %u MiB on disk %u\n", nslots * PAGE_SIZE / (1024 * 1024), i - 1);

	// Boot cpu for now
	swapd_task = kthread_create(swapd, NULL, 1ULL << (boot_cpu - cpus));
}

bool swap_enabled(void)
{
	return swap_disk != NULL;
}

// The next slot or 0 when swap is full
static size_t slot_alloc(size_t n)
{
	size_t first, slot = swap_cursor, len = 0;

	// One lap around
	for (size_t i = 0; i < nslots + n - 2; ++i) {
		if (++slot >= nslots) slot = 1, len = 0;
		len = swap_map[slot] ? 0 : len + 1;
		if (len < n) continue;

		first = slot - n + 1;
		for (i = first; i <= slot; ++i) atomic_inc(&swap_map[i]);
		swap_cursor = slot;
		return first;
	}

	return 0;
}

// Drops the references of the swap pages
void swap_free(physaddr_t entry, size_t npages)
{
	for (size_t i = 0; i < npages; ++i) {
		uint16_t old = atomic_dec(&swap_map[swap_slot(entry) + i]);

		assert(old > 0);
	}
}

static int share_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	*entry = *(physaddr_t *)walker->udata;
	return 0;
}

// Maps the swap entry at va too, like for a forked child
int swap_share(struct page_table *pml4, uintptr_t va, physaddr_t entry, size_t npages)
{
	struct page_walker walker = {
		.pdpte_callback = ptbl_alloc,
		.pml4e_callback = ptbl_alloc,
		.udata = &entry,
	};
	// A huge page has its swap entry in the PDE
	if (npages == 1) {
		walker.pte_callback = share_pte;
		walker.pde_callback = ptbl_alloc;
	} else {
		walker.pde_callback = share_pte;
	}

	int ret = walk_page_range(pml4, (void *)va, (void *)(va + npages * PAGE_SIZE), &walker);
	for (size_t i = 0; ret == 0 && i < npages; ++i) atomic_inc(&swap_map[swap_slot(entry) + i]);
	return ret;
}

struct swap_found {
	physaddr_t *entry;
	size_t npages;
};

static int find_swap_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct swap_found *found = walker->udata;

	if (pte_swapped(*entry)) {
		found->entry = entry;
		found->npages = (end - base + 1) / PAGE_SIZE;
	}
	return 0;
}

// The swap entry of the page at va, or NULL if the page is not in swap
physaddr_t *swap_lookup(struct task *task, void *va, size_t *npages)
{
	void *base = (void *)ROUNDDOWN((uintptr_t)va, PAGE_SIZE);
	struct swap_found found = { 0 };
	struct page_walker walker = {
		.pte_callback = find_swap_pte,
		.pde_callback = find_swap_pte,
		.udata = &found,
	};

	walk_page_range(task->task_pml4, base, base + PAGE_SIZE, &walker);
	if (npages) *npages = found.npages;
	return found.entry;
}

// Find huge page around va and va+len to later get split if needed
void *swap_huge_partial(struct task *task, void *va, size_t len)
{
	uintptr_t base = (uintptr_t)va, end = base + len - 1;
	uintptr_t ends[] = { base, end };
	size_t i, npages;

	for (i = 0; i < 2; ++i) {
		uintptr_t huge = ROUNDDOWN(ends[i], HPAGE_SIZE);
		if (swap_lookup(task, (void *)ends[i], &npages) && npages == PAGE_TABLE_ENTRIES && (huge < base || huge + HPAGE_SIZE - 1 > end)) return (void *)huge;
	}

	return NULL;
}

// Splits the swapped out huge page at va into a page table of 512 swap entries, without swapping it in
int swap_split(struct task *task, void *va)
{
	physaddr_t *entry = swap_lookup(task, va, NULL);
	struct page_info *page = page_alloc(0);
	struct page_table *ptbl;
	size_t i;

	if (!page) return -ENOMEM;
	atomic_inc(&page->pp_ref);

	ptbl = page2kva(page);
	for (i = 0; i < PAGE_TABLE_ENTRIES; ++i) ptbl->entries[i] = swap_pte(swap_slot(*entry) + i, *entry);

	*entry = page2pa(page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	return 0;
}

static void swapd_wake(void)
{
	if (swapd_task->task_status == TASK_NOT_RUNNABLE) sched_enqueue(swapd_task);
}

// Queue the current task on swapd and sleep until swapd served it, does not return
static void swapd_queue_task(void *va, void *end)
{
	struct task *task = cur_task;

	assert(task->task_type == TASK_TYPE_USER);

	task->task_swap_va = va;
	task->task_swap_end = end;
	task->task_karma += read_tsc() - task->task_start_tsc;

	// Leave the address space before swapd may change it
	cur_task = NULL;
	load_pml4(PADDR(kernel_pml4));

	fine_spin_lock(&swap_lock);
	task->task_status = TASK_SWAPPING;
	list_add_tail(&swap_waiters, &task->task_node);
	swapd_wake();
	fine_spin_unlock(&swap_lock);

	sched_yield();
}

void swap_wait(void *va, bool in_syscall)
{
	if (in_syscall) cur_task->task_frame.rip -= SYSCALL_INSN_SIZE; // put the current user stack in a loop, while we yield to somebody else
	swapd_queue_task(va, NULL);
}


void swap_out_range(void *va, size_t len)
{
	cur_task->task_frame.rax = 0;
	swapd_queue_task(va, va + len);
}

// The task goes away, so swapd should stop
void swap_cancel(struct task *task)
{
	fine_spin_lock(&swap_lock);
	if (task->task_status == TASK_SWAPPING) list_del(&task->task_node);
	if (io.task == task) io.task = NULL;
	fine_spin_unlock(&swap_lock);
}

static void swap_wake(struct task *task)
{
	list_del(&task->task_node);
	sched_enqueue(task);
}

struct clock {
	struct task *task;
	bool force; // Evict without a second chance, for MADV_SWAP
	size_t slot, npages;
	bool stop; // write all to disk due to upcoming huge page
	// Next in this eviction walk
	uintptr_t next;
};

static bool clock_room(struct clock *clock) // more place before we move to disk
{
	size_t slot = clock->slot + clock->npages;

	return !clock->stop && (!clock->npages || (clock->npages < SWAP_CLUSTER && slot < nslots && !swap_map[slot]));
}

static size_t clock_slot(struct clock *clock)
{
	if (!clock->npages) return clock->slot = slot_alloc(1);

	atomic_inc(&swap_map[clock->slot + clock->npages]);
	return clock->slot + clock->npages;
}

static bool clock_referenced(physaddr_t *entry, uintptr_t base, struct clock *clock)
{
	if (!(*entry & PAGE_ACCESSED)) return false;
	*entry &= ~PAGE_ACCESSED; // second chance
	return true;
}

// Keep the page where rstatus is returned in
static bool clock_pinned(uintptr_t base, uintptr_t end, struct clock *clock)
{
	struct task *task = clock->task;
	uintptr_t rstatus = (uintptr_t)task->task_rstatus;

	return task->task_status == TASK_NOT_RUNNABLE && rstatus && base <= rstatus && rstatus <= end;
}

static int swap_page_pte(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker)
{
	struct clock *clock = walker->udata;
	struct page_info *page;
	size_t slot;

	if (!(*entry & PAGE_PRESENT) || (!clock->force && clock_referenced(entry, base, clock)) || clock_pinned(base, end, clock)) return 0;
	page = pa2page(PAGE_ADDR(*entry));
	if (page->pp_ref > 1) return 0; // do not swap CoW pages for now. With reverse mappings could be done.
	if (!(slot = clock_slot(clock))) return -ENOMEM;

	memcpy(page2kva(bounce + clock->npages++), page2kva(page), PAGE_SIZE);

	*entry = swap_pte(slot, *entry);
	tlb_invalidate(clock->task->task_pml4, (void *)base);
	page_decref(page);

	clock->next = end + 1;
	return -EAGAIN;
}

static int swap_huge(physaddr_t *entry, uintptr_t base, uintptr_t end, struct clock *clock)
{
	struct page_info *page = pa2page(PAGE_ADDR(*entry));

	if (!(clock->slot = slot_alloc(PAGE_TABLE_ENTRIES))) return 0;
	memcpy(page2kva(bounce), page2kva(page), HPAGE_SIZE);


	*entry = swap_pte(clock->slot, *entry);
	tlb_invalidate(clock->task->task_pml4, (void *)base);
	page_decref(page);

	clock->npages = PAGE_TABLE_ENTRIES;
	clock->next = end + 1;
	return -EAGAIN;
}

// Swap out a huge page
static int swap_page_pde(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker)
{
	struct clock *clock = walker->udata;
	struct page_info *page;

	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_HUGE) || (!clock->force && clock_referenced(entry, base, clock)) || clock_pinned(base, end, clock)) return 0;
	page = pa2page(PAGE_ADDR(*entry));
	if (page->pp_ref != 1) return 0;

	if (!clock->npages) return swap_huge(entry, base, end, clock);

	clock->stop = true;
	clock->next = base;
	return -EAGAIN;
}

// Evict the first page in [base, end) to the bounce page, -EAGAIN once evicted, -ENOMEM when swap is full
static int swap_page_walk(struct clock *clock, uintptr_t base, uintptr_t end)
{
	struct page_walker walker = {
		.pte_callback = swap_page_pte,
		.pde_callback = swap_page_pde,
		.udata = clock,
	};
	int ret = walk_page_range(clock->task->task_pml4, (void *)base, (void *)end, &walker);

	// Under pressure so release the pages we have freed till now
	page_zero_flush();
#if defined(BONUS_MULTI_CORE_FRAME_ALLOCATOR) || defined(BONUS_LAB6)
	page_cache_flush();
#endif
	return ret;
}

static bool clock_blocked(struct task *task)
{
	return task->task_type == TASK_TYPE_USER && !task->task_killed && (task->task_status == TASK_SWAPPING || task->task_status == TASK_NOT_RUNNABLE);
}

static bool clock_lock(struct task *task, bool *busy)
{
	if (!task || !clock_blocked(task)) return false;
	if (!fine_spin_trylock(&task->task_lock)) {
		*busy = true;
		return false;
	}
	if (clock_blocked(task)) return true;

	fine_spin_unlock(&task->task_lock);
	return false;
}

// Pick a page using CLOCK, returns the slot to write it to or 0 if none is left
static void clock_evict(struct clock *clock, bool *busy)
{
	size_t visits;
	int ret = 0;

	for (visits = 0; visits <= 2 * (size_t)pid_max && ret != -ENOMEM && clock_room(clock); ++visits) {
		clock->task = tasks[hand.pid];
		if (clock_lock(clock->task, busy)) {
			while (clock_room(clock) && (ret = swap_page_walk(clock, hand.va, USER_LIM)) == -EAGAIN)hand.va = clock->next;
			fine_spin_unlock(&clock->task->task_lock);
			if (ret == -EAGAIN || ret == -ENOMEM) break;
		}

		hand.pid = hand.pid % (pid_max - 1) + 1;
		hand.va = 0;
	}
}

static void swapd_write(size_t slot, size_t npages)
{
	io.op = IO_WRITE;
	io.page = bounce;
	io.slot = slot;
	io.npages = npages;
	io.done = 0;
	swap_rw();
}

static void swapd_read(struct page_info *page, size_t slot, size_t npages, struct task *task)
{
	io.op = IO_READ;
	io.page = page;
	io.slot = slot;
	io.npages = npages;
	io.done = 0;
	io.task = task;
	swap_rw();
}

static size_t swapd_reclaim(void)
{
	struct clock clock = { 0 };
	bool busy = false;

	clock_evict(&clock, &busy);
	if (clock.npages) swapd_write(clock.slot, clock.npages);
	else if (!busy) swapd_oom = true;

	return clock.npages;
}

// Swap out the next page of the MADV_SWAP range
static void swapd_swap_out(struct task *task)
{
	struct clock clock = { .task = task, .force = true };
	uintptr_t va = (uintptr_t)task->task_swap_va;
	int ret;

	while (clock_room(&clock) && (ret = swap_page_walk(&clock, va, (uintptr_t)task->task_swap_end)) == -EAGAIN) va = clock.next;

	if (clock.npages) {
		// Take turns with the other tasks that need to be swapped
		task->task_swap_va = (void *)va;
		list_del(&task->task_node);
		list_add_tail(&swap_waiters, &task->task_node);
		swapd_write(clock.slot, clock.npages);
		return;
	}

	if (ret == -ENOMEM) task->task_frame.rax = -ENOMEM;
	swap_wake(task);
}

// Finish the disk command, false while the disk is busy
static bool swapd_poll(void)
{
	physaddr_t *entry;
	size_t npages;
	int ret;

	if (io.op == IO_NONE) return true;
	if ((ret = swap_rw()) == -EAGAIN) return false;

	if (ret < 0) panic("swap: cannot %s slot %u", io.op == IO_WRITE ? "write" : "read", io.slot + io.done);

	// Issue the next chunk
	if ((io.done += io_chunk()) < io.npages) {
		swap_rw();
		return false;
	}

	if (io.op == IO_WRITE) {
		// The page went to swap when it was copied to the bounce page
	} else if (!io.task) { // Read, but the task went away in the meanwhile
		page_free(io.page);
	} else if (!(entry = swap_lookup(io.task, io.task->task_swap_va, &npages)) || npages != io.npages) {
		page_free(io.page);
		sched_enqueue(io.task);
	} else {
		swap_free(*entry, npages);
		atomic_inc(&io.page->pp_ref);
		*entry = page2pa(io.page) | (*entry & (PAGE_UMASK | PAGE_DIRTY)) | PAGE_PRESENT | PAGE_ACCESSED | (npages == PAGE_TABLE_ENTRIES ? PAGE_HUGE : 0); // "Access" the page
		sched_enqueue(io.task);
	}

	io.op = IO_NONE;
	io.task = NULL;
	return true;
}

//swap out a page of its range, or read a page back
static void swapd_serve(void)
{
	struct task *task = container_of(list_head(&swap_waiters), struct task, task_node);
	struct page_info *page;
	physaddr_t *entry;
	size_t npages;

	if (task->task_swap_end) {
		swapd_swap_out(task);
		return;
	}

	// The page is no longer in swap, so the task can just retry and get some memory
	if (!(entry = swap_lookup(task, task->task_swap_va, &npages))) {
		if (swapd_batch < SWAP_BATCH && (npages = swapd_reclaim())) { // work in batches
			swapd_batch += npages;
			return;
		}
		swapd_batch = 0;
		swap_wake(task);
		return;
	}
	// get page to swap back in, otherwise reclaim
	if (!(page = page_alloc(npages == PAGE_TABLE_ENTRIES ? ALLOC_HUGE : 0))) {
		swapd_reclaim();
		return;
	}

	list_del(&task->task_node);
	swapd_read(page, swap_slot(*entry), npages, task);
}

static void swapd(void *arg)
{
	for (;;) {
		asm volatile("cli" ::: "memory");
		big_spin_lock(&kernel_lock);
		fine_spin_lock(&swap_lock);

		if (swapd_poll() && !list_is_empty(&swap_waiters)) swapd_serve();

		// Sleep when idle
		if (!swapd_oom && io.op == IO_NONE && list_is_empty(&swap_waiters)) {
			cur_task->task_status = TASK_NOT_RUNNABLE;
			cur_task = NULL;
		}

		fine_spin_unlock(&swap_lock);

		if (swapd_oom) {
			swapd_oom = false;
			oom_kill();
		}

		sched_yield();
	}
}
