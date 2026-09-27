
#include <types.h>
#include <error.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

#ifdef BONUS_ZERO_DEDUP
static struct page_info *zero_page = NULL;

static int task_zero_fault(struct task *task, struct vma *vma, void *va) {
	if (zero_page == NULL) {
		zero_page = page_alloc(ALLOC_ZERO);
		if (!zero_page) return -ENOMEM;
		zero_page->pp_ref++; // Trick to avoid COW change this page in future
	}

	return page_insert(task->task_pml4, zero_page, (void *)ROUNDDOWN((uintptr_t)va, PAGE_SIZE), PAGE_PRESENT | PAGE_USER | (vma->vm_flags & VM_EXEC ? 0 : PAGE_NO_EXEC));
}
#endif

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

	#ifdef BONUS_ZERO_DEDUP
	// Only read or exec should be catched here. write will get handled by populate_vma_range
	if (!(flags & PF_PRESENT) && !(flags & PF_WRITE) && !vma->vm_src) return task_zero_fault(task, vma, va);
	#endif

	return populate_vma_range(task, (void *)ROUNDDOWN((uintptr_t)va, PAGE_SIZE),PAGE_SIZE, vma_flags);

}
