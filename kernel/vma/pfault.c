
#include <types.h>
#include <error.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

static int task_cow_fault(struct task *task, void *va) {
	physaddr_t *entry = NULL;
	struct page_info *page = page_lookup(task->task_pml4, va, &entry); if (!page) return -EFAULT;

	uint32_t size = (*entry & PAGE_HUGE) ? HPAGE_SIZE : PAGE_SIZE;
	void *base = (void *)ROUNDDOWN((uintptr_t)va, size);
	if (page->pp_ref == 1) {
		*entry |= PAGE_WRITE;
		tlb_invalidate(task->task_pml4, base);
		return 0;
	}

	struct page_info *new_page = page_alloc((size == HPAGE_SIZE) ? ALLOC_HUGE : 0); if (!new_page) return -ENOMEM;
	memcpy(page2kva(new_page), page2kva(page), size);

	return page_insert(task->task_pml4, new_page, base, (*entry & PAGE_UMASK) | PAGE_WRITE);
}


/* Handles the page fault for a given task. */
int task_page_fault_handler(struct task *task, void *va, int flags)
{
	struct vma *vma;
	int vma_flags = VM_READ;

	if (!task || va >= (void *)USER_LIM)
		return -EFAULT;

	vma = task_find_vma(task, va);
	if (!vma)
		return -EFAULT;

	if (flags & PF_WRITE)
		vma_flags |= VM_WRITE;
	if (flags & PF_IFETCH) {
		vma_flags &= ~VM_READ;
		vma_flags |= VM_EXEC;
	}

	if ((vma->vm_flags & vma_flags) != vma_flags) return -EPERM;

	if ((flags & PF_PRESENT) && (flags & PF_WRITE)) return task_cow_fault(task, va);

	return populate_vma_range(task, (void *)ROUNDDOWN((uintptr_t)va, PAGE_SIZE),PAGE_SIZE, vma_flags);

}
