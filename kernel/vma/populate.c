
#include <types.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Checks the flags in udata against the flags of the VMA to check appropriate
 * permissions. If the permissions are all right, this function populates the
 * address range [base, base + size) with physical pages. If the VMA is backed
 * by an executable, the data is copied over. Then the protection of the
 * physical pages is adjusted to match the permissions of the VMA.
 */
int do_populate_vma(struct task *task, void *base, size_t size,
	struct vma **vma, void *udata)
{
	int flags = *(int *)udata;
	uintptr_t start = (uintptr_t)base;
	uintptr_t end = start + size;
	uintptr_t vma_start = (uintptr_t)(*vma)->vm_base;
	uintptr_t vma_end = (uintptr_t)(*vma)->vm_end;
	uint64_t page_flags = PAGE_PRESENT | PAGE_USER;

	if (((*vma)->vm_flags & flags) != flags)
		return -1;

	if (start < vma_start){
		start = vma_start;
	}
	if (end > vma_end){
		end = vma_end;
	}

	if ((*vma)->vm_flags & VM_WRITE){
		page_flags |= PAGE_WRITE;
	}
	if (!((*vma)->vm_flags & VM_EXEC)){
		page_flags |= PAGE_NO_EXEC;
	}

	populate_region(task->task_pml4, (void *)start, end - start, page_flags);

	if ((*vma)->vm_src && (*vma)->vm_len) {
		uintptr_t data_base = (uintptr_t)(*vma)->vm_base + (*vma)->vm_offset;
		uintptr_t data_end = data_base + (*vma)->vm_len;
		uintptr_t cursor = start > data_base ? start : data_base;
		uintptr_t stop = end < data_end ? end : data_end;

		while (cursor < stop) {
			struct page_info *page;
			physaddr_t *entry;
			size_t page_size, offset, chunk;

			page = page_lookup(task->task_pml4, (void *)cursor, &entry);

			if (!page) return -1;

			page_size = (*entry & PAGE_HUGE) ? HPAGE_SIZE : PAGE_SIZE;
			offset = cursor & (page_size - 1);
			chunk = page_size - offset > stop - cursor ? stop - cursor : page_size - offset;

			memcpy((char *)page2kva(page) + offset, (char *)(*vma)->vm_src + (cursor - data_base), chunk);

			cursor += chunk;
		}
	}

	protect_region(task->task_pml4, (void *)start, end - start, page_flags);

	return 0;
}

/* Populates the VMAs for the given address range [base, base + size) by
 * backing the VMAs with physical pages.
 */
int populate_vma_range(struct task *task, void *base, size_t size, int flags)
{
	return walk_vma_range(task, base, size, do_populate_vma, &flags);
}
