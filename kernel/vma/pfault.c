
#include <types.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Handles the page fault for a given task. */
int task_page_fault_handler(struct task *task, void *va, int flags)
{
	struct vma *vma;
	int vma_flags = VM_READ;

	if (!task || va >= (void *)USER_LIM)
		return -1;

	vma = task_find_vma(task, va);
	if (!vma)
		return -1;

	if (flags & PF_WRITE)
		vma_flags |= VM_WRITE;
	if (flags & PF_IFETCH) {
		vma_flags &= ~VM_READ;
		vma_flags |= VM_EXEC;
	}

	if ((vma->vm_flags & vma_flags) != vma_flags){
		return -1;
	}

	return populate_vma_range(task, (void *)ROUNDDOWN((uintptr_t)va, PAGE_SIZE),PAGE_SIZE, vma_flags);
}
