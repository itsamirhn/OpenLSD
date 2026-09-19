
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
		uintptr_t cursor = start;
		uintptr_t data_offset = start - vma_start;

		while (cursor < end && data_offset < (*vma)->vm_len) {
			physaddr_t *entry;
			struct page_info *page;
			size_t copy_size = PAGE_SIZE - (cursor & (PAGE_SIZE - 1));

			if (copy_size > end - cursor){
				copy_size = end - cursor;
			}
			if (copy_size > (*vma)->vm_len - data_offset){
				copy_size = (*vma)->vm_len - data_offset;
			}

			page = page_lookup(task->task_pml4, (void *)cursor, &entry);
			if (!page){
				return -1;
			}
			memcpy((char *)page2kva(page) + (cursor & (PAGE_SIZE - 1)),(char *)(*vma)->vm_src + (*vma)->vm_offset + data_offset,copy_size);

			cursor += copy_size;
			data_offset += copy_size;
		}
	}

	return 0;
}

/* Populates the VMAs for the given address range [base, base + size) by
 * backing the VMAs with physical pages.
 */
int populate_vma_range(struct task *task, void *base, size_t size, int flags)
{
	return walk_vma_range(task, base, size, do_populate_vma, &flags);
}
