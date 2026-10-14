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

static struct disk *swap_disk;
static size_t sectors_per_page;

static uint16_t swap_map[SWAP_MAX_SLOTS];
static size_t nslots;

static struct page_info *bounce; // copy page

// The disk state
static struct {
	enum { IO_NONE, IO_READ, IO_WRITE } op;
	struct page_info *page;
	size_t slot;
} io;

// Issue command and poll disk
static int swap_rw(void)
{
	void *buf = page2kva(io.page);
	uint64_t sector = io.slot * sectors_per_page;
	int64_t ret;

	ret = io.op == IO_WRITE ? disk_write(swap_disk, buf, sectors_per_page, sector) : disk_read(swap_disk, buf, sectors_per_page, sector);

	if (ret == -EAGAIN) return ret;
	return ret == PAGE_SIZE ? 0 : -EFAULT;
}

// Find swap disk
void swap_init(void)
{
	struct disk_stat stat;
	size_t i;
	int ret;

	if (!(bounce = page_alloc(0))) return;
	atomic_inc(&bounce->pp_ref);

	io.op = IO_READ;
	io.page = bounce;
	io.slot = 0;

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
}

bool swap_enabled(void)
{
	return swap_disk != NULL;
}

void swap_free(physaddr_t entry)
{
	uint16_t old = atomic_dec(&swap_map[swap_slot(entry)]);

	assert(old > 0);
}

static int share_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	*entry = *(physaddr_t *)walker->udata;
	return 0;
}

// Maps the swap entry at va too, like for a forked child
int swap_share(struct page_table *pml4, uintptr_t va, physaddr_t entry)
{
	struct page_walker walker = {
		.pte_callback = share_pte,
		.pde_callback = ptbl_alloc,
		.pdpte_callback = ptbl_alloc,
		.pml4e_callback = ptbl_alloc,
		.udata = &entry,
	};
	int ret = walk_page_range(pml4, (void *)va, (void *)(va + PAGE_SIZE), &walker);

	if (ret == 0) atomic_inc(&swap_map[swap_slot(entry)]);
	return ret;
}
