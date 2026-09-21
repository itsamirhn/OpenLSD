
#include <types.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Changes the protection flags of the given VMA. Does nothing if the flags
 * would remain the same. Splits up the VMA into the address range
 * [base, base + size) and changes the protection of the physical pages backing
 * the VMA. Then attempts to merge the VMAs in case the protection became the
 * same as that of any of the adjacent VMAs.
 */
int do_protect_vma(struct task *task, void *base, size_t size, struct vma **vma,
	void *udata)
{
	struct vma *current = *vma;
	struct vma *target = split_vmas(task, current, base, size);
	uintptr_t start = (uintptr_t)base;
	uintptr_t end = start + size;
	int flags = *(int *)udata;

	if (base > current->vm_base && target == current){
		return -1; //split_vmas failed
	} 

	target->vm_flags = flags;
	protect_region(task->task_pml4, target->vm_base,target->vm_end - target->vm_base,
		(flags & VM_WRITE ? PAGE_WRITE : 0) | (flags & VM_EXEC ? 0 : PAGE_NO_EXEC) | (flags ? PAGE_USER : 0));

	*vma = merge_vmas(task, target);
	return 0;
}

/* Changes the protection flags of the VMAs for the given address range
 * [base, base + size).
 */
int protect_vma_range(struct task *task, void *base, size_t size, int flags)
{
	return walk_vma_range(task, base, size, do_protect_vma, &flags);
}
